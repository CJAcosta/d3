/**
 * The gear catalog: the teaching content for each item. Sizes and priorities
 * are personalized in src/lib/plan.ts; this file holds what stays the same.
 *
 * Price ranges are rough category ballparks for youth gear (USD), not prices
 * of any specific product. Do not replace them with live retailer prices:
 * Amazon does not allow showing prices that are not pulled from its API.
 */
import type { Budget, GearId } from '../lib/types';

export interface GearInfo {
  id: GearId;
  name: string;
  /** Ballpark slang, shown for fun next to the name. */
  nickname?: string;
  icon: string;
  why: string;
  fitCheck: string[];
  lookFor: string[];
  mistakes: string[];
  price: Record<Budget, [number, number]>;
  tier: Record<Budget, string>;
  guide?: { href: string; label: string };
}

export const BUDGET_LABEL: Record<Budget, string> = {
  lean: 'Keep it lean',
  balanced: 'Balanced',
  allin: 'All in',
};

export const gear: Record<GearId, GearInfo> = {
  glove: {
    id: 'glove',
    name: 'Fielding glove',
    nickname: 'the leather',
    icon: 'glove',
    why: 'The one piece of gear every player needs, at every practice. A glove that fits is the difference between catching the ball and being scared of it.',
    fitCheck: [
      'They can open and squeeze the glove shut with one hand. If they need two hands to close it, it’s too big or too stiff.',
      'Fingers reach into the finger slots without much empty space at the tips.',
      'Point the glove at the ground and shake. It should stay on. Tighten the wrist strap if it has one.',
    ],
    lookFor: [
      'A “youth pattern” or “youth fit” label: narrower finger slots for smaller hands.',
      'Soft, “game-ready” leather or synthetic. Stiff adult leather can take weeks to break in.',
      'An adjustable wrist strap (Velcro) for kids under 10.',
    ],
    mistakes: [
      'Buying one to “grow into.” An oversized glove is the #1 reason young kids drop the ball.',
      'Getting the wrong hand. RHT means Right-Hand Throw, so the glove goes on the LEFT hand.',
    ],
    price: { lean: [20, 40], balanced: [40, 80], allin: [80, 160] },
    tier: {
      lean: 'A soft synthetic or entry leather glove is perfect for a first season.',
      balanced: 'Real leather that breaks in fast. Lasts two or three seasons if it fits now.',
      allin: 'Premium youth leather. Worth it for a committed player who will oil and care for it.',
    },
    guide: { href: '/guides/youth-glove-size-chart/', label: 'Glove size chart by age and position' },
  },
  bat: {
    id: 'bat',
    name: 'Bat',
    nickname: 'the lumber',
    icon: 'bat',
    why: 'Many teams share bats, but a bat that’s the right length and weight for your kid makes hitting much easier, and hitting is what keeps kids in the game.',
    fitCheck: [
      'Chest test: put the knob in the center of their chest and point the bat out to the side. Their fingertips should reach the end of the barrel.',
      'Side test: stand the bat next to their leg. The knob should come up to the middle of their palm with their arm hanging down.',
      'Weight test: have them hold the bat straight out to the side by the handle for about 20 seconds. If it dips or wobbles, it’s too heavy.',
    ],
    lookFor: [
      'The correct certification stamp for your league, printed on the barrel or taper.',
      'One-piece aluminum is durable and affordable. Composite and hybrid bats cost more and need warm-up hits in cold weather.',
      'A comfortable grip. Thinner handles help small hands.',
    ],
    mistakes: [
      'Buying a bat with the wrong stamp. Your league won’t allow it, and most stores won’t take back a used bat.',
      'Going too long or too heavy “to grow into.” A heavy bat ruins a young swing.',
      'Using a tee ball bat with real baseballs. Tee ball bats are only for soft tee balls.',
    ],
    price: { lean: [25, 60], balanced: [60, 150], allin: [150, 350] },
    tier: {
      lean: 'A one-piece aluminum bat with the right stamp. Does everything a first-year hitter needs.',
      balanced: 'A newer-model alloy or hybrid bat. A bit more pop and less sting on mishits.',
      allin: 'A flagship composite bat. Only worth it for an experienced hitter who will use it for a full season or more.',
    },
    guide: { href: '/guides/youth-bat-size-chart/', label: 'Bat size chart and fit tests' },
  },
  helmet: {
    id: 'helmet',
    name: 'Batting helmet',
    nickname: 'the lid',
    icon: 'helmet',
    why: 'Required for every batter and base runner. Teams often have shared helmets, but a shared one that wobbles doesn’t protect, and kids hit more confidently in their own.',
    fitCheck: [
      'Snug, not squeezing. Have them shake their head “no” and “yes.” The helmet should move with their head, not slide around.',
      'The front sits about one finger-width above the eyebrows.',
      'The ear holes line up with their ears, and they can see straight ahead without tipping it back.',
    ],
    lookFor: [
      'The NOCSAE stamp. Little League requires NOCSAE-approved helmets.',
      'Double ear flaps (both sides). Youth leagues require them.',
      'A face guard, or a helmet that accepts one.',
    ],
    mistakes: [
      'Guessing the size. Measure their head first. It takes 30 seconds with a string.',
      'Adding stickers or paint without checking the maker’s instructions. Some can void the NOCSAE certification.',
      'Keeping a helmet with a crack or loose padding. Replace it.',
    ],
    price: { lean: [25, 40], balanced: [40, 70], allin: [70, 130] },
    tier: {
      lean: 'A basic NOCSAE helmet with a face guard included protects just as well for a first season.',
      balanced: 'Better padding and adjustable fit. Many include a removable face guard.',
      allin: 'Lighter shell, more ventilation and a custom-feeling fit system.',
    },
    guide: { href: '/guides/batting-helmet-size-guide/', label: 'How to measure for a batting helmet' },
  },
  cleats: {
    id: 'cleats',
    name: 'Cleats',
    nickname: 'spikes',
    icon: 'cleat',
    why: 'Grass and dirt are slippery. Cleats help them start, stop and run the bases without falling. Little League asks players to wear cleats at outdoor practices.',
    fitCheck: [
      'Leave about a thumb’s width (half an inch) between the big toe and the front of the shoe.',
      'Try them on with the thick socks they’ll play in, ideally in the afternoon when feet are biggest.',
      'Heel shouldn’t slip when they jog. Have them run a few steps in the store or the hallway.',
    ],
    lookFor: [
      'Molded (rubber or plastic) cleats for ages 12 and under.',
      'Mid-cut if your kid likes extra ankle support; low-cut if they want light and fast.',
      'Sizing in the same size as their sneakers. Kids sizes run 10C–13C, then 1Y–7Y.',
    ],
    mistakes: [
      'Buying more than a half size up “to grow into.” Loose cleats cause blisters and trips.',
      'Metal spikes for younger players. Not allowed in Little League Majors and below.',
    ],
    price: { lean: [25, 40], balanced: [40, 65], allin: [65, 100] },
    tier: {
      lean: 'Basic molded cleats. Kids outgrow them every season anyway.',
      balanced: 'More comfortable, more durable molded cleats from a major brand.',
      allin: 'Top youth models. Lighter, with better support for kids who play year-round.',
    },
    guide: { href: '/guides/youth-baseball-cleats/', label: 'Cleats guide: molded vs metal' },
  },
  pants: {
    id: 'pants',
    name: 'Baseball pants',
    icon: 'pants',
    why: 'Usually part of the uniform. They protect knees and legs when sliding and fielding grounders. Little League recommends baseball pants or sweatpants, not shorts, even at practice.',
    fitCheck: [
      'Waist fits with a belt and doesn’t slide down when they squat.',
      'They can squat and lunge without the pants pulling tight at the knees.',
      'Full-length pants should stop at the top of the cleat.',
    ],
    lookFor: [
      'The color your team asks for. Ask before buying (white and gray are most common).',
      'Reinforced or double knees. They last much longer.',
      'Belt loops. Most teams wear a belt.',
    ],
    mistakes: [
      'Buying before the coach tells you the color and style (full-length, knicker or open-bottom).',
      'Only buying one pair. Get two: one for practice, one for games.',
    ],
    price: { lean: [12, 20], balanced: [20, 30], allin: [30, 50] },
    tier: {
      lean: 'Basic polyester pants. Buy two.',
      balanced: 'Heavier fabric with reinforced knees.',
      allin: 'Stretch fabric that moves better and stains less.',
    },
    guide: { href: '/guides/youth-baseball-gear-checklist/', label: 'The full first-season checklist' },
  },
  beltsocks: {
    id: 'beltsocks',
    name: 'Belt & socks',
    icon: 'socks',
    why: 'Small items that most uniforms need, and the ones parents most often forget until game day.',
    fitCheck: ['Belt fits snugly through the pants loops.', 'Socks come up over the calf.'],
    lookFor: ['Team colors. Ask the coach.', 'Elastic belts are easier for little kids to fasten.'],
    mistakes: ['Waiting until the night before the first game.'],
    price: { lean: [10, 18], balanced: [15, 25], allin: [20, 35] },
    tier: {
      lean: 'A basic elastic belt and a two-pack of socks.',
      balanced: 'Belt plus a few pairs of thicker baseball socks.',
      allin: 'Cushioned performance socks that last the season.',
    },
  },
  protection: {
    id: 'protection',
    name: 'Protective cup & shorts',
    icon: 'shield',
    why: 'A hard-hit grounder or a wild pitch can hurt. Little League requires a cup for catchers and recommends an athletic supporter or compression shorts for boys.',
    fitCheck: [
      'The cup sits snug in the pouch of compression shorts or a jock and doesn’t shift when they run.',
      'Compression shorts fit tight but don’t dig in at the waist or leg.',
    ],
    lookFor: [
      'Compression shorts with a built-in cup pocket. Easier for kids than a jockstrap.',
      'Sliding shorts with padded hips if they’ll be sliding into bases.',
      'Sizing by age or waist on the maker’s chart. Cups come in peewee, youth and adult.',
    ],
    mistakes: [
      'Skipping it because they’re young. Getting used to wearing one early makes it no big deal later.',
      'Buying an adult size. It won’t sit right and kids won’t want to wear it.',
    ],
    price: { lean: [12, 20], balanced: [20, 35], allin: [35, 55] },
    tier: {
      lean: 'A youth cup and compression short combo pack.',
      balanced: 'A flexible-edge cup with sliding shorts.',
      allin: 'Premium sliding shorts plus a top-protection cup.',
    },
  },
  water: {
    id: 'water',
    name: 'Water bottle',
    icon: 'bottle',
    why: 'Practices are long and fields are sunny. A big bottle with their name on it is the cheapest must-have on this list.',
    fitCheck: ['They can open and close it themselves with one hand.'],
    lookFor: ['24–32 oz, insulated so it stays cold.', 'A clip or loop that hooks onto a bag or the dugout fence.'],
    mistakes: ['Not labeling it. Every dugout has five identical bottles.'],
    price: { lean: [8, 15], balanced: [15, 30], allin: [25, 45] },
    tier: {
      lean: 'Any big, leak-proof bottle.',
      balanced: 'An insulated steel bottle.',
      allin: 'A large insulated jug for all-day tournaments.',
    },
  },
  bag: {
    id: 'bag',
    name: 'Bat bag / backpack',
    icon: 'bag',
    why: 'Keeps the glove, helmet, bat and water together, and lets your kid carry their own gear, which is good for them.',
    fitCheck: ['Bat sleeves hold their bat with the handle sticking out.', 'They can carry it full by themselves.'],
    lookFor: [
      'Backpack style for younger kids. Easy to carry from the car.',
      'A fence hook so it hangs in the dugout, plus a helmet clip or pocket.',
      'A wheeled bag only if they catch (catcher’s gear is heavy).',
    ],
    mistakes: ['A huge bag a small kid can’t carry.'],
    price: { lean: [20, 35], balanced: [35, 60], allin: [60, 110] },
    tier: {
      lean: 'A simple backpack with bat sleeves and a fence hook.',
      balanced: 'Separate pockets for cleats and valuables, plus a helmet holder.',
      allin: 'Bigger, sturdier, with room for catcher’s gear or tournament weekends.',
    },
  },
  battinggloves: {
    id: 'battinggloves',
    name: 'Batting gloves',
    icon: 'battinggloves',
    why: 'Not required, but they improve grip and take the sting out of cold-weather hits. Many kids also feel more confident wearing them.',
    fitCheck: [
      'Snug with no loose material at the fingertips.',
      'They can put them on and fasten the strap themselves.',
    ],
    lookFor: ['Youth sizing by hand length on the maker’s chart.', 'A Velcro wrist strap.'],
    mistakes: ['Buying adult sizes. Floppy gloves hurt grip instead of helping.'],
    price: { lean: [12, 20], balanced: [20, 35], allin: [35, 55] },
    tier: {
      lean: 'A basic youth pair.',
      balanced: 'Better leather palms that last a season.',
      allin: 'The same gloves the big leaguers wear, in youth sizes.',
    },
  },
  catchersgear: {
    id: 'catchersgear',
    name: 'Catcher’s gear set',
    nickname: 'the tools of ignorance',
    icon: 'mask',
    why: 'Catchers need a helmet with a face mask and throat guard, a chest protector, leg guards, and a cup (boys). Leagues often have a shared set, but one that fits protects better.',
    fitCheck: [
      'Chest protector: measure from the base of the throat to the belly button. That’s the chest protector length.',
      'Leg guards: measure from the middle of the kneecap to the top of the ankle. The knee cap should sit on the knee.',
      'Helmet: same head measurement as a batting helmet. It must not shift when they pop up from a squat.',
    ],
    lookFor: [
      'An age-sized boxed set. It’s the best value and everything fits together.',
      'The NOCSAE stamp on the helmet. Also look for the NOCSAE chest-protector seal, which high school rules require.',
      'A throat guard. Little League requires one.',
    ],
    mistakes: [
      'Buying adult gear to grow into. Gaps in coverage are where injuries happen.',
      'Forgetting the cup. It’s required for catchers.',
    ],
    price: { lean: [90, 140], balanced: [140, 250], allin: [250, 450] },
    tier: {
      lean: 'A youth boxed set from a major brand covers everything.',
      balanced: 'A step-up set with lighter, better-ventilated pieces.',
      allin: 'Pro-style pieces for a full-time catcher, often bought separately.',
    },
    guide: { href: '/guides/catchers-gear-for-kids/', label: 'Catcher’s gear sizing for kids' },
  },
  catchersmitt: {
    id: 'catchersmitt',
    name: 'Catcher’s mitt',
    icon: 'mitt',
    why: 'A catcher’s mitt has extra padding to take pitch after pitch. Most leagues expect catchers to use one once kids are pitching.',
    fitCheck: ['They can close it around a ball with one hand.', 'Their hand doesn’t slide around inside.'],
    lookFor: ['A youth mitt, sized by circumference (29½–32½″ for most kids).', 'A pre-broken-in or “game-ready” pocket.'],
    mistakes: ['A stiff adult mitt. Kids can’t close it.'],
    price: { lean: [35, 60], balanced: [60, 120], allin: [120, 250] },
    tier: {
      lean: 'An entry youth mitt.',
      balanced: 'A leather youth mitt with a fast break-in.',
      allin: 'A premium mitt for a full-time catcher.',
    },
    guide: { href: '/guides/catchers-gear-for-kids/', label: 'Catcher’s gear sizing for kids' },
  },
  firstbasemitt: {
    id: 'firstbasemitt',
    name: 'First base mitt',
    icon: 'mitt',
    why: 'A longer, scoop-shaped mitt that helps a first baseman dig out low throws.',
    fitCheck: ['They can close it one-handed.', 'It doesn’t slide off when they reach.'],
    lookFor: ['A youth first base mitt (11½–12″ for most kids under 13).'],
    mistakes: ['Buying one before they’re actually playing first base regularly.'],
    price: { lean: [35, 60], balanced: [60, 120], allin: [120, 250] },
    tier: {
      lean: 'An entry youth mitt.',
      balanced: 'Leather with a quick break-in.',
      allin: 'Premium leather for a regular first baseman.',
    },
    guide: { href: '/guides/youth-glove-size-chart/', label: 'Glove and mitt size chart' },
  },
  practice: {
    id: 'practice',
    name: 'Backyard practice kit',
    icon: 'tee',
    why: 'Ten minutes of catch or tee work at home does more for a beginner than anything you can buy. A tee and a bucket of balls is all it takes.',
    fitCheck: ['The tee adjusts low enough for their knees and high enough for their chest.'],
    lookFor: [
      'An adjustable batting tee.',
      'Softer “reduced injury” balls for young kids and backyard practice.',
      'A rebounder net if they want to practice throwing and catching alone.',
    ],
    mistakes: ['Hard baseballs for very young kids at home. Softer training balls build confidence.'],
    price: { lean: [20, 40], balanced: [40, 90], allin: [90, 200] },
    tier: {
      lean: 'A basic tee and a dozen soft training balls.',
      balanced: 'A sturdier tee plus a hitting or pitch-back net.',
      allin: 'A pro-style tee, net and ball bucket for daily practice.',
    },
  },
  glovecare: {
    id: 'glovecare',
    name: 'Glove break-in kit',
    icon: 'oil',
    why: 'A new glove is stiff. A little conditioner and a ball tied in the pocket overnight help it close easily, so it’s ready for the first practice.',
    fitCheck: ['After a week of catch, the glove closes easily with one hand.'],
    lookFor: ['Glove conditioner made for baseball gloves.', 'A glove wrap or a wide rubber band plus an old ball.'],
    mistakes: ['Microwaving, oven-baking or soaking a glove. It ruins the leather.'],
    price: { lean: [8, 15], balanced: [12, 25], allin: [20, 35] },
    tier: {
      lean: 'A small bottle of glove conditioner.',
      balanced: 'Conditioner plus a glove wrap.',
      allin: 'A full care kit with mallet, wrap and conditioner.',
    },
    guide: { href: '/guides/how-to-break-in-a-glove/', label: 'How to break in a glove (the kid-friendly way)' },
  },
  sunglasses: {
    id: 'sunglasses',
    name: 'Sports sunglasses',
    icon: 'sunglasses',
    why: 'Fly balls and afternoon sun don’t mix. Helpful for outfielders and sunny fields.',
    fitCheck: ['Stays put when they shake their head or run.'],
    lookFor: ['Shatter-resistant lenses and a wrap-around fit.', 'Youth sizing.'],
    mistakes: ['Fashion sunglasses. They can break on impact.'],
    price: { lean: [12, 25], balanced: [20, 40], allin: [40, 90] },
    tier: {
      lean: 'Youth sport sunglasses with impact-resistant lenses.',
      balanced: 'Polarized youth sport glasses.',
      allin: 'Premium sport optics in youth sizes.',
    },
  },
};
