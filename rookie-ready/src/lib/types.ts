export type Hand = 'R' | 'L' | 'unsure';
export type BatSide = 'R' | 'L' | 'S' | 'unsure';
export type Team = 'teeball' | 'rec' | 'travel' | 'school' | 'unsure';
export type Position = 'unsure' | 'infield' | 'outfield' | 'pitcher' | 'catcher' | 'firstbase';
export type Budget = 'lean' | 'balanced' | 'allin';
export type Experience = 'first' | 'returning';
export type Protection = 'cup' | 'pelvic' | 'skip';
/** Things the family already owns or the team hands out. */
export type Have = 'glove' | 'bat' | 'helmet' | 'cleats' | 'pants' | 'catcher';

export interface Answers {
  /** Optional first name or nickname. Never leaves the device. */
  name?: string;
  age: number;
  heightIn?: number;
  weightLb?: number;
  throws: Hand;
  bats: BatSide;
  team: Team;
  position: Position;
  experience: Experience;
  budget: Budget;
  have: Have[];
  protection: Protection;
  /** Head circumference in inches, if the parent measured. */
  headIn?: number;
  /** Shoe size as the parent typed it, e.g. "2Y" or "13C". */
  shoe?: string;
}

/** Where the player is in the baseball journey. Drives most rules. */
export type Stage = 'teeball' | 'coachpitch' | 'kidpitch' | 'teen' | 'highschool';

export type BatCert = 'usa-teeball' | 'usa' | 'usssa' | 'usa-or-bbcor' | 'bbcor';

export type GearId =
  | 'glove'
  | 'bat'
  | 'helmet'
  | 'cleats'
  | 'pants'
  | 'beltsocks'
  | 'protection'
  | 'water'
  | 'bag'
  | 'battinggloves'
  | 'catchersgear'
  | 'catchersmitt'
  | 'firstbasemitt'
  | 'practice'
  | 'glovecare'
  | 'sunglasses';

export type Priority = 'must' | 'recommended' | 'nice';
