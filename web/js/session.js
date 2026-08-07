// The session state machine, ported from src/c/session.c. This file owns
// truth; windows render it. localStorage stands in for the persist keys,
// and — the one thing the web can't promise — a Notification stands in
// for the Wakeup API: if the tab is alive it fires on time, but a closed
// tab is a dead kettle. The watch keeps that superpower.

export const PH = { NONE: 0, READY: 1, STEEPING: 2, ALARM: 3 };
export const INCR_UNSET = -32768;
export const STEEP_MIN_S = 10;
export const STEEP_MAX_S = 20 * 60;
export const INCR_MIN_S = -60;
export const INCR_MAX_S = 120;
const ALARM_CAP_S = 45;

const KEY_SESSION = 'brewsession.session';
const KEY_RECENTS = 'brewsession.recents';
const KEY_OPTS = 'brewsession.opts';
export const RECENTS_MAX = 6;

export const session = blank();
export const opts = { auto_open: false, pour_s: 5 };
let recents = [];

let listener = null;
let alarmHook = null;
let heartbeatTimer = null;
let alarmTimer = null;
let alarmStarted = 0;
let audioCtx = null;

function blank() {
  return { phase: PH.NONE, infusion: 0, base_s: 0,
           increment_s: INCR_UNSET, override_s: 0, end_epoch: 0 };
}

const now = () => Math.floor(Date.now() / 1000);

// ---- persistence -----------------------------------------------------------

function save() { localStorage.setItem(KEY_SESSION, JSON.stringify(session)); }
function saveRecents() { localStorage.setItem(KEY_RECENTS, JSON.stringify(recents)); }
export function saveOpts() { localStorage.setItem(KEY_OPTS, JSON.stringify(opts)); }

function load(key, fallback) {
  try { return JSON.parse(localStorage.getItem(key)) ?? fallback; }
  catch { return fallback; }
}

// ---- recents ---------------------------------------------------------------

export const recentsCount = () => recents.length;
export const recentsGet = (i) => recents[i];

// Move-to-front with a dedup that understands "+ ?" (see session.c).
function recentsTouch(base_s, increment_s) {
  let found = -1;
  for (let i = 0; i < recents.length; i++) {
    const r = recents[i];
    if (r.base_s !== base_s) continue;
    if (r.increment_s === increment_s ||
        r.increment_s === INCR_UNSET || increment_s === INCR_UNSET) {
      found = i;
      if (increment_s === INCR_UNSET) increment_s = r.increment_s;
      break;
    }
  }
  if (found >= 0) recents.splice(found, 1);
  recents.unshift({ base_s, increment_s });
  if (recents.length > RECENTS_MAX) recents.length = RECENTS_MAX;
  saveRecents();
}

// ---- steep math ------------------------------------------------------------

function steepFor(infusion, base_s, increment_s) {
  if (infusion > 1 && increment_s === INCR_UNSET) return -1;
  let s = base_s + (infusion - 1) * (infusion > 1 ? increment_s : 0);
  return Math.min(STEEP_MAX_S, Math.max(STEEP_MIN_S, s));
}

export function steepS() {
  if (session.override_s > 0) return session.override_s;
  return steepFor(session.infusion, session.base_s, session.increment_s);
}

export function needsIncrement() {
  return session.infusion > 1 && session.increment_s === INCR_UNSET &&
         session.override_s === 0;
}

export function remainingS() {
  if (session.phase !== PH.STEEPING) return 0;
  return Math.max(0, session.end_epoch - now());
}

// ---- alarm side effects ----------------------------------------------------
// The wrist's escalating vibe becomes vibrate() where the device has it and
// a small square-wave beep everywhere, same stages, same 45s give-up.

export function playPattern(pattern) {
  if (navigator.vibrate) navigator.vibrate(pattern);
  try {
    audioCtx = audioCtx || new AudioContext();
    let t = audioCtx.currentTime;
    for (let i = 0; i < pattern.length; i++) {
      const dur = pattern[i] / 1000;
      if (i % 2 === 0) {
        const osc = audioCtx.createOscillator();
        const gain = audioCtx.createGain();
        osc.type = 'square';
        osc.frequency.value = 180;
        gain.gain.value = 0.05;
        osc.connect(gain).connect(audioCtx.destination);
        osc.start(t);
        osc.stop(t + dur);
      }
      t += dur;
    }
  } catch { /* no audio, no matter */ }
}

function alarmBuzz() {
  const elapsed = now() - alarmStarted;
  if (session.phase !== PH.ALARM || elapsed >= ALARM_CAP_S) {
    alarmTimer = null;
    return;   // gave up quietly; the alarm screen stays
  }
  const GENTLE = [150, 100, 150];
  const FIRM = [200, 100, 200, 100, 350];
  const LOUD = [400, 150, 400, 150, 600];
  playPattern(elapsed < 10 ? GENTLE : elapsed < 25 ? FIRM : LOUD);
  alarmTimer = setTimeout(alarmBuzz, 1600);
}

function alarmBegin() {
  session.phase = PH.ALARM;
  session.end_epoch = 0;
  save();
  alarmStarted = now();
  alarmBuzz();
  if (document.hidden && Notification?.permission === 'granted') {
    new Notification(`Infusion ${session.infusion} done`,
                     { body: 'tea time', tag: 'brewsession' });
  }
  if (alarmHook) alarmHook();
}

function alarmEnd() {
  if (alarmTimer) { clearTimeout(alarmTimer); alarmTimer = null; }
  if (navigator.vibrate) navigator.vibrate(0);
}

// ---- heartbeat -------------------------------------------------------------

function heartbeat() {
  if (session.phase === PH.STEEPING && now() >= session.end_epoch) alarmBegin();
  if (listener) listener();
}

function heartbeatSync() {
  const want = session.phase === PH.STEEPING || session.phase === PH.ALARM;
  if (want && !heartbeatTimer) heartbeatTimer = setInterval(heartbeat, 250);
  if (!want && heartbeatTimer) { clearInterval(heartbeatTimer); heartbeatTimer = null; }
}

export function setListener(cb) { listener = cb; }
export function setAlarmHook(hook) { alarmHook = hook; }

// ---- transitions -----------------------------------------------------------

export function live() { return session.phase !== PH.NONE; }

export function sessionNew(base_s, increment_s) {
  alarmEnd();
  Object.assign(session, blank());
  session.phase = PH.READY;
  session.infusion = 1;
  session.base_s = base_s;
  session.increment_s = increment_s;
  recentsTouch(base_s, increment_s);
  save();
  heartbeatSync();
}

export function setIncrement(s) {
  session.increment_s = s;
  recentsTouch(session.base_s, s);
  save();
}

export function adjustOnce(s) { session.override_s = s; save(); }

export function skip() {
  session.infusion++;
  session.override_s = 0;
  save();
}

export function startSteep() {
  const steep = steepS();
  if (steep < 0) return;
  session.phase = PH.STEEPING;
  session.end_epoch = now() + steep;
  save();
  heartbeatSync();
  // The polite moment to ask for the wakeup stand-in.
  if (Notification && Notification.permission === 'default') {
    Notification.requestPermission();
  }
}

export function adjustRunning(ds) {
  if (session.phase !== PH.STEEPING) return;
  session.end_epoch = Math.max(now() + 1, session.end_epoch + ds);
  save();
}

export function dismiss() {
  alarmEnd();
  session.phase = PH.READY;   // dismiss and advance are one gesture
  session.infusion++;
  session.override_s = 0;
  session.end_epoch = 0;
  save();
  heartbeatSync();
  if (listener) listener();
}

export function abandon() {
  alarmEnd();
  Object.assign(session, blank());
  save();
  heartbeatSync();
}

export function init() {
  recents = load(KEY_RECENTS, []);
  Object.assign(opts, load(KEY_OPTS, {}));
  const s = load(KEY_SESSION, null);
  if (s && typeof s.phase === 'number') Object.assign(session, s);
  // Reconcile with the wall clock: a steep that ended in our absence is an
  // alarm now; a persisted mid-alarm rings again, briefly.
  if (session.phase === PH.STEEPING && now() >= session.end_epoch) alarmBegin();
  else if (session.phase === PH.ALARM) alarmBegin();
  heartbeatSync();
}
