#!/usr/bin/env python3
"""Boot Nebula OS in QEMU and drive its shell over the serial console.

Each case sends a command and asserts on what comes back, so the whole
kernel -- boot sector, paging, scheduler, filesystem, shell -- is
exercised end to end rather than in isolation.
"""

import os
import subprocess
import sys
import time

IMAGE = os.environ.get("NEBULA_IMAGE", "build/nebula.img")
PROMPT = "$ "
BOOT_TIMEOUT = 25.0
CMD_TIMEOUT = 12.0


class Machine:
    def __init__(self, image):
        self.proc = subprocess.Popen(
            ["qemu-system-i386", "-fda", image, "-display", "none",
             "-serial", "stdio", "-no-reboot", "-monitor", "none"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            bufsize=0,
        )
        os.set_blocking(self.proc.stdout.fileno(), False)
        self.buf = ""

    def read_until(self, needle, timeout):
        """Accumulate output until `needle` appears. Returns all of it."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                chunk = self.proc.stdout.read()
            except (OSError, ValueError):
                chunk = None
            if chunk:
                self.buf += chunk.decode("utf-8", errors="replace")
                if needle in self.buf:
                    return self.buf
            else:
                if self.proc.poll() is not None:
                    break
                time.sleep(0.02)
        return self.buf

    def send(self, line):
        self.proc.stdin.write((line + "\r").encode())
        self.proc.stdin.flush()

    def command(self, line, timeout=CMD_TIMEOUT):
        """Run a shell command and return only its output."""
        self.buf = ""
        self.send(line)
        out = self.read_until(PROMPT, timeout)
        # Drop the echoed command and the trailing prompt.
        if "\n" in out:
            out = out.split("\n", 1)[1]
        return out.rsplit(PROMPT, 1)[0]

    def kill(self):
        try:
            self.proc.kill()
            self.proc.wait(timeout=5)
        except Exception:
            pass


def parse_proposals(listing):
    """Pull {id: [body lines]} out of an `auto` listing."""
    found = {}
    current = None

    for line in listing.splitlines():
        stripped = line.strip()

        if stripped.startswith("[") and "]" in stripped:
            current = stripped[1:stripped.index("]")]
            found[current] = []
        elif current and line.startswith("      "):
            found[current].append(stripped)
        elif not stripped:
            current = None

    return found


def find_proposal(machine, needle):
    """The id of the pending proposal whose body contains `needle`."""
    for pid, body in parse_proposals(machine.command("auto")).items():
        if any(needle in line for line in body):
            return pid
    return None


class Results:
    def __init__(self):
        self.passed = 0
        self.failed = 0

    def check(self, name, ok, detail=""):
        if ok:
            self.passed += 1
            print("  PASS  %s" % name)
        else:
            self.failed += 1
            print("  FAIL  %s" % name)
            if detail:
                for line in detail.strip().splitlines():
                    print("          | %s" % line)

    def contains(self, name, haystack, needle):
        self.check(name, needle in haystack,
                   "expected %r in:\n%s" % (needle, haystack))

    def missing(self, name, haystack, needle):
        self.check(name, needle not in haystack,
                   "did not expect %r in:\n%s" % (needle, haystack))


def main():
    if not os.path.exists(IMAGE):
        print("error: %s not found; run make first" % IMAGE)
        return 1

    r = Results()
    m = Machine(IMAGE)

    try:
        print("boot:")
        boot = m.read_until("Type `help` to get started", BOOT_TIMEOUT)

        r.contains("kernel banner", boot, "Nebula OS 0.1.0")
        r.contains("protected mode reached", boot, "[gdt] flat descriptors loaded")
        r.contains("PIC remapped", boot, "[idt] 256 vectors installed")
        r.contains("physical memory detected", boot, "[pmm]")
        r.contains("paging enabled", boot, "[paging] identity mapped")
        r.contains("kernel heap up", boot, "[heap]")
        r.contains("scheduler up", boot, "[sched] round-robin")
        r.contains("filesystem mounted", boot, "[fs] in-memory filesystem")
        r.check("boot self-tests all passed",
                "0 failed" in boot and "[FAIL]" not in boot,
                boot)
        r.contains("shell started", boot, "Welcome to Nebula OS")

        m.read_until(PROMPT, 5)

        print("\nshell:")
        out = m.command("uname")
        r.contains("uname reports the kernel", out, "Nebula 0.1.0 i386")

        out = m.command("echo hello world")
        r.contains("echo joins its arguments", out, "hello world")

        out = m.command("ls")
        r.contains("ls shows /home", out, "home/")
        r.contains("ls shows /tmp", out, "tmp/")
        r.contains("ls shows /dev", out, "dev/")

        out = m.command("cat /home/readme.txt")
        r.contains("cat prints a seeded file", out, "A 32-bit x86 kernel written from scratch")

        out = m.command("pwd")
        r.contains("pwd starts at the root", out, "/")

        m.command("mkdir work")
        out = m.command("cd work")
        r.missing("cd into a new directory succeeds", out, "no such file")

        out = m.command("pwd")
        r.contains("pwd follows cd", out, "/work")

        m.command("write notes.txt first line")
        out = m.command("cat notes.txt")
        r.contains("write then cat round-trips", out, "first line")

        m.command("append notes.txt second line")
        out = m.command("cat notes.txt")
        r.check("append adds without losing the original",
                "first line" in out and "second line" in out, out)

        out = m.command("ls")
        r.contains("ls shows the new file", out, "notes.txt")

        out = m.command("cd ..")
        out = m.command("pwd")
        r.check("cd .. returns to the parent", out.strip().endswith("/"), out)

        out = m.command("rm work")
        r.contains("rm refuses a non-empty directory", out, "not empty")

        m.command("rm work/notes.txt")
        out = m.command("rm work")
        r.missing("rm removes the directory once empty", out, "not empty")

        out = m.command("ls")
        r.missing("the removed directory is gone", out, "work/")

        out = m.command("cat /nope")
        r.contains("cat reports a missing file", out, "no such file or directory")

        out = m.command("bogus-command")
        r.contains("unknown commands are reported", out, "command not found")

        out = m.command("mem")
        r.contains("mem reports physical memory", out, "physical memory")
        r.contains("mem reports the heap", out, "kernel heap")

        out = m.command("memmap")
        r.contains("memmap shows the BIOS map", out, "usable")

        out = m.command("ps")
        r.contains("ps lists the shell task", out, "shell")
        r.contains("ps lists the kernel task", out, "kernel")

        out = m.command("uptime")
        r.contains("uptime reports timer ticks", out, "timer ticks at 100 Hz")

        out = m.command("date")
        r.check("date reads the RTC", "UTC" in out and "20" in out, out)

        out = m.command("help")
        r.contains("help lists commands", out, "spawn")

        # A spawned task must actually be scheduled while the shell waits.
        m.command("spawn 2")
        out = m.command("sleep 1200", timeout=15)
        r.contains("a spawned task runs concurrently", out, "[worker")
        r.contains("the spawned task finishes", out, "finished")

        # --- the automation system -------------------------------------
        #
        # Repeat a sequence and the OS should work out the shape of it,
        # offer it back as a command, and then actually run it.
        #
        # By this point the machine has a history: everything above was
        # real activity and the engine has been watching all of it. The
        # checks below therefore look for *this* discovery among whatever
        # else the system has legitimately noticed, rather than assuming
        # it is the only thing on the OS's mind.
        print("\nself-programming:")

        for name in ("alpha", "beta"):
            m.command("mkdir %s" % name)
            m.command("cd %s" % name)
            m.command("cd ..")

        r.check("the sequence is not yet proposed after two repetitions",
                find_proposal(m, "mkdir $1") is None)

        m.command("mkdir gamma")
        m.command("cd gamma")
        out = m.command("cd ..")
        r.missing("a fresh discovery is not announced in the same breath",
                  out, "mkdir $1")

        out = m.command("uname")
        r.contains("the repetition is noticed", out, "you have done this 3 times")
        r.contains("the proposal is generalised", out, "mkdir $1")
        r.contains("the parameter is reused across steps", out, "cd $1")
        r.contains("it says how many arguments it needs", out, "taking 1 argument")
        r.contains("a concrete example is shown", out, "mkdir gamma")

        listing = m.command("auto")
        r.contains("the watchlist is visible", listing, "watching")
        r.contains("the counts are shown", listing, "3x")

        proposal_id = find_proposal(m, "mkdir $1")
        r.check("the proposal is listed with an id",
                proposal_id is not None, listing)

        out = m.command("accept %s project" % proposal_id)
        r.contains("accepting names the new command", out, "learned `project`")
        r.contains("it is saved where it can be read", out, "/auto/project")

        r.check("it is no longer pending once accepted",
                find_proposal(m, "mkdir $1") is None)
        r.contains("it now appears as an automation", m.command("auto"), "project")

        out = m.command("cat /auto/project")
        r.contains("the automation is readable as a file", out, "mkdir $1")
        r.contains("it records where it came from", out,
                   "learned from 3 repetitions")

        # Running it must actually perform every step.
        m.command("project delta")
        out = m.command("ls")
        r.contains("running the automation does the work", out, "delta/")

        out = m.command("pwd")
        r.check("the automation leaves the shell where it should",
                out.strip().endswith("/"), out)

        out = m.command("project")
        r.contains("an automation states what it needs", out, "needs 1 argument")

        out = m.command("history 40")
        r.contains("the journal records actions", out, "actions recorded")
        r.contains("the journal shows what was done", out, "mkdir gamma")
        r.contains("an automation is journalled as itself", out, "project delta")
        r.missing("its expanded steps are not journalled separately",
                  out, "mkdir delta")

        # Asking the system about itself must not become something it
        # observes, or inspecting it would teach it to inspect itself.
        for _ in range(4):
            m.command("help")

        out = m.command("history 12")
        r.missing("inspecting the system is not itself recorded", out, "help")

        out = m.command("forget project")
        r.contains("an automation can be forgotten", out, "forgot `project`")

        out = m.command("project epsilon")
        r.contains("a forgotten automation is gone", out, "command not found")

        out = m.command("ls /auto")
        r.missing("its file is removed too", out, "project")

        # Deliberately fault: this should panic, not reboot or hang.
        print("\nfault handling:")
        m.buf = ""
        m.send("fault")
        out = m.read_until("System halted", 10)
        r.contains("a bad access raises a page fault", out, "KERNEL PANIC")
        r.contains("the fault address is reported", out, "0x40000000")
        r.contains("the fault is diagnosed", out, "page not present")
        r.contains("the kernel halts cleanly", out, "System halted")

    finally:
        m.kill()

    print("\n%d passed, %d failed" % (r.passed, r.failed))
    return 1 if r.failed else 0


if __name__ == "__main__":
    sys.exit(main())
