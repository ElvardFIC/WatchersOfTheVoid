//! Connection admission, shared by both binaries: a pool with a total cap and a per-address cap. A
//! slot is taken at ACCEPT -- before a byte is read or a TLS handshake starts -- and is given back
//! when it drops, which also happens on an unwind: a panicking connection task cannot leak one.
//!
//! Why at accept: a cap checked inside the request handler counts only the connections that have
//! already finished their TLS handshake. A peer that opened sockets and never sent a ClientHello
//! was counted by nothing and bounded only by the process's file limit. The per-address cap is what
//! stops ONE source from filling the whole pool; the address is coarsened by `ip_bucket` (a full
//! IPv4 address, an IPv6 /64), like every other per-source control in this crate.

use crate::common::{env_int, env_str, ip_bucket, log};
use std::collections::{HashMap, VecDeque};
use std::sync::{LazyLock, Mutex};
use std::time::{Duration, Instant};

/// A refusal is logged at most this often per pool. Every refused socket used to cost a
/// synchronous log write on the runtime's worker -- one worker on a 1-vCPU box -- at whatever rate
/// the flood connected, so the line now carries how many refusals it stands for.
const REFUSAL_LOG_EVERY: Duration = Duration::from_secs(1);

pub struct Pool {
    name: &'static str,
    max_total: usize,
    max_per_addr: u32,
    // One lock over both counts, so a slot is taken or refused against a consistent pair. An entry
    // exists only while it holds a slot, so the map never outgrows `max_total`.
    held: Mutex<Held>,
    refusals: Mutex<Refusals>,
}

#[derive(Default)]
struct Held {
    total: usize,
    per_addr: HashMap<String, u32>,
}

#[derive(Default)]
struct Refusals {
    last_logged: Option<Instant>,
    unlogged: u64,
}

/// One admitted connection. Dropping it gives the slot back.
pub struct Slot<'a> {
    pool: &'a Pool,
    key: String,
}

impl Pool {
    pub fn new(name: &'static str, max_total: usize, max_per_addr: u32) -> Pool {
        Pool {
            name,
            max_total,
            max_per_addr,
            held: Mutex::new(Held::default()),
            refusals: Mutex::new(Refusals::default()),
        }
    }

    /// Admit a connection from `ip`, or say which cap refused it.
    pub fn admit(&self, ip: &str) -> Result<Slot<'_>, String> {
        let key = ip_bucket(ip);
        let mut held = self.held.lock().unwrap_or_else(|e| e.into_inner());
        if held.total >= self.max_total {
            return Err(format!("{} cap ({})", self.name, self.max_total));
        }
        let n = held.per_addr.get(&key).copied().unwrap_or(0);
        if n >= self.max_per_addr {
            return Err(format!("per-address {} cap ({})", self.name, self.max_per_addr));
        }
        held.per_addr.insert(key.clone(), n + 1);
        held.total += 1;
        Ok(Slot { pool: self, key })
    }

    /// `admit`, with a refusal logged at most once per `REFUSAL_LOG_EVERY`; the line that is
    /// written counts the refusals since the last one.
    pub fn admit_or_log(&self, ip: &str) -> Option<Slot<'_>> {
        match self.admit(ip) {
            Ok(slot) => Some(slot),
            Err(why) => {
                if let Some(n) = self.note_refusal(Instant::now()) {
                    let tail = if n > 1 { format!(" ({n} refusals since the last line)") } else { String::new() };
                    log(&format!("[{ip}] refused: {why}{tail}"));
                }
                None
            }
        }
    }

    /// Count one refusal; `Some(n)` when a line is due, `n` being the refusals it reports.
    fn note_refusal(&self, now: Instant) -> Option<u64> {
        note(&mut self.refusals.lock().unwrap_or_else(|e| e.into_inner()), now)
    }

    /// Slots held in the whole pool.
    pub fn held(&self) -> usize {
        self.held.lock().unwrap_or_else(|e| e.into_inner()).total
    }

    /// Slots held by `ip`'s address bucket.
    pub fn held_by(&self, ip: &str) -> u32 {
        let held = self.held.lock().unwrap_or_else(|e| e.into_inner());
        held.per_addr.get(&ip_bucket(ip)).copied().unwrap_or(0)
    }

    #[cfg(test)]
    fn addresses(&self) -> usize {
        self.held.lock().unwrap().per_addr.len()
    }
}

/// MTA's flood guard for its HTTP server, at both servers' accepts: `CHTTPD::ShouldAllowConnection`
/// keeps a `CConnectHistory` of 20 connects per 10 s per address and ignores an address that goes
/// past it for a minute (CHTTPD.cpp:40, CMainConfig.cpp:66). A source that churns connections then
/// costs no pool slot and no TLS handshake. A browser opens three connections and refreshes every
/// 5 s, a host heartbeats every 30 s, so 20 per 10 s admits about ten open browsers behind one
/// address; raise it for a known shared address, or exclude it. Both are settings, as in MTA
/// (`httpdosthreshold`, `httpdosexclude`, CMainConfig.cpp:247-252). The same shape guards game
/// joins (coop/net/connect_history.cpp).
pub struct ConnectHistory {
    // 0 turns the guard off.
    max: usize,
    period: Duration,
    ban: Duration,
    max_tracked: usize,
    // `ip_bucket` keys of the addresses that are always allowed and never tracked.
    exclude: Vec<String>,
    state: Mutex<History>,
}

#[derive(Default)]
struct History {
    per_addr: HashMap<String, Recent>,
    last_sweep: Option<Instant>,
    refusals: Refusals,
    // Newcomers admitted untracked because the table was full.
    untracked: Refusals,
}

#[derive(Default)]
struct Recent {
    connects: VecDeque<Instant>,
    ignored_until: Option<Instant>,
}

/// Addresses tracked at once: ~640 bytes each at 20 connects (tests/conn_cost.rs), so ~6.3 MiB at
/// most. Past it a new address is admitted untracked (the pools' per-address shares still hold
/// it): refusing it instead would let a source-diverse flood lock every newcomer out.
const MAX_TRACKED: usize = 10_000;

/// The flood guard's default: connects allowed per period. `COOP_CONNECT_FLOOD_MAX` overrides it.
const DEFAULT_FLOOD_MAX: i64 = 20;

/// The one history each server process keeps, for every listener it runs. Built from
/// `COOP_CONNECT_FLOOD_MAX` (0 turns the guard off) and `COOP_CONNECT_FLOOD_EXCLUDE` (a comma
/// separated list of addresses that are never limited) the first time it is used.
pub static CONNECTS: LazyLock<ConnectHistory> = LazyLock::new(|| {
    let max = usize::try_from(env_int("COOP_CONNECT_FLOOD_MAX", DEFAULT_FLOOD_MAX))
        .unwrap_or(DEFAULT_FLOOD_MAX as usize);
    ConnectHistory::new(max, Duration::from_secs(10), Duration::from_secs(60), MAX_TRACKED)
        .with_exclude(&env_str("COOP_CONNECT_FLOOD_EXCLUDE", ""))
});

impl ConnectHistory {
    pub fn new(max: usize, period: Duration, ban: Duration, max_tracked: usize) -> ConnectHistory {
        ConnectHistory {
            max,
            period,
            ban,
            max_tracked,
            exclude: Vec::new(),
            state: Mutex::new(History::default()),
        }
    }

    /// Never limit or track the addresses in `list`, comma separated.
    pub fn with_exclude(mut self, list: &str) -> ConnectHistory {
        self.exclude = list.split(',').map(str::trim).filter(|a| !a.is_empty()).map(ip_bucket).collect();
        self
    }

    /// Whether a connect from `ip` at `now` is admitted. The one past `max` inside `period` is
    /// refused and starts the address's ignored minute, logged once. The key is `ip_bucket` (an
    /// IPv6 /64), where the mod's own port keys the full address.
    pub fn allow(&self, ip: &str, now: Instant) -> bool {
        if self.max == 0 {
            return true;
        }
        let key = ip_bucket(ip);
        if self.exclude.contains(&key) {
            return true;
        }
        let (allowed, line) = self.decide(key, ip, now);
        // The line is written after the lock is gone: a log write is a syscall.
        if let Some(line) = line {
            log(&line);
        }
        allowed
    }

    /// `allow`'s decision under the lock, and the log line it owes, if any.
    fn decide(&self, key: String, ip: &str, now: Instant) -> (bool, Option<String>) {
        let mut h = self.state.lock().unwrap_or_else(|e| e.into_inner());
        self.sweep(&mut h, now);
        if !h.per_addr.contains_key(&key) && h.per_addr.len() >= self.max_tracked {
            // MTA's map is unbounded (CConnectHistory.h:14); this one is bounded for the unit's
            // memory and fails open, logged once per line-second.
            let line = note(&mut h.untracked, now).map(|n| {
                let tail = if n > 1 { format!(" ({n} since the last line)") } else { String::new() };
                format!("[{ip}] connect history full ({} addresses): admitted untracked{tail}", self.max_tracked)
            });
            return (true, line);
        }
        let recent = h.per_addr.entry(key).or_default();
        if recent.ignored_until.is_some_and(|until| now < until) {
            return (false, None);
        }
        recent.ignored_until = None;
        while recent.connects.front().is_some_and(|t| now.duration_since(*t) >= self.period) {
            recent.connects.pop_front();
        }
        if recent.connects.len() < self.max {
            recent.connects.push_back(now);
            return (true, None);
        }
        // MTA keeps the stamps through the ban and can re-ban on them in the sweep gap
        // (CConnectHistory.cpp:51-57,107-111); the stamps are cleared here so a ban does not.
        recent.connects.clear();
        recent.ignored_until = Some(now + self.ban);
        // The address's open connections are left alone: the pools bound them, where MTA's EHS
        // stops them (ehs.cpp:977-991) and a registered relay peer must not pay for a neighbour.
        let line = note(&mut h.refusals, now).map(|n| {
            let tail = if n > 1 { format!(" ({n} addresses since the last line)") } else { String::new() };
            format!(
                "[{ip}] connect flood: more than {} inside {} s, ignored for {} s{tail}",
                self.max,
                self.period.as_secs(),
                self.ban.as_secs()
            )
        });
        (false, line)
    }

    /// At most once a second, forget the addresses that are neither ignored nor recent.
    fn sweep(&self, h: &mut History, now: Instant) {
        if h.last_sweep.is_some_and(|t| now.duration_since(t) < Duration::from_secs(1)) {
            return;
        }
        h.last_sweep = Some(now);
        let period = self.period;
        h.per_addr.retain(|_, r| {
            r.ignored_until.is_some_and(|until| now < until)
                || r.connects.back().is_some_and(|t| now.duration_since(*t) < period)
        });
    }

    #[cfg(test)]
    fn tracked(&self) -> usize {
        self.state.lock().unwrap().per_addr.len()
    }
}

/// One more refusal into `r`: `Some(n)` when a line is due, `n` being the refusals it reports.
fn note(r: &mut Refusals, now: Instant) -> Option<u64> {
    r.unlogged += 1;
    let due = r.last_logged.map_or(true, |t| now.duration_since(t) >= REFUSAL_LOG_EVERY);
    if !due {
        return None;
    }
    r.last_logged = Some(now);
    Some(std::mem::take(&mut r.unlogged))
}

impl Drop for Slot<'_> {
    fn drop(&mut self) {
        let mut held = self.pool.held.lock().unwrap_or_else(|e| e.into_inner());
        held.total = held.total.saturating_sub(1);
        if let Some(n) = held.per_addr.get_mut(&self.key) {
            *n = n.saturating_sub(1);
            if *n == 0 {
                held.per_addr.remove(&self.key);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{ConnectHistory, Pool, REFUSAL_LOG_EVERY};
    use std::time::{Duration, Instant};

    #[test]
    fn a_connect_flood_is_ignored_for_the_ban_and_alone() {
        let h = ConnectHistory::new(3, Duration::from_secs(10), Duration::from_secs(60), 100);
        let t0 = Instant::now();
        assert!((0..3).all(|i| h.allow("203.0.113.7", t0 + Duration::from_millis(i))), "under the cap");
        assert!(!h.allow("203.0.113.7", t0 + Duration::from_secs(1)), "the fourth inside the period passed");
        assert!(h.allow("198.51.100.1", t0 + Duration::from_secs(1)), "another address paid for the flood");
        assert!(!h.allow("203.0.113.7", t0 + Duration::from_secs(59)), "admitted inside the ban");
        assert!(h.allow("203.0.113.7", t0 + Duration::from_secs(62)), "still ignored after the ban");
    }

    #[test]
    fn connects_age_out_of_the_period() {
        let h = ConnectHistory::new(2, Duration::from_secs(10), Duration::from_secs(60), 100);
        let t0 = Instant::now();
        assert!(h.allow("192.0.2.1", t0) && h.allow("192.0.2.1", t0 + Duration::from_secs(5)));
        assert!(h.allow("192.0.2.1", t0 + Duration::from_secs(11)), "a connect older than the period still counted");
        // The sweep forgets an address with nothing recent and no ban.
        assert!(h.allow("192.0.2.9", t0 + Duration::from_secs(30)));
        assert_eq!(h.tracked(), 1, "a stale address outlived the sweep");
    }

    #[test]
    fn an_excluded_address_is_allowed_past_the_max_and_leaves_no_entry() {
        let h = ConnectHistory::new(2, Duration::from_secs(10), Duration::from_secs(60), 100)
            .with_exclude(" 192.0.2.77 , ,2001:db8:1:2::5");
        let t0 = Instant::now();
        assert!((0..10).all(|_| h.allow("192.0.2.77", t0)), "an excluded address was limited");
        assert!((0..10).all(|_| h.allow("2001:db8:1:2::ffff", t0)), "an excluded /64 was limited");
        assert_eq!(h.tracked(), 0, "an excluded address was tracked");
        assert!(h.allow("192.0.2.1", t0) && h.allow("192.0.2.1", t0));
        assert!(!h.allow("192.0.2.1", t0), "the exclude list spared another address");
    }

    #[test]
    fn a_max_of_zero_allows_everything() {
        let h = ConnectHistory::new(0, Duration::from_secs(10), Duration::from_secs(60), 100);
        let t0 = Instant::now();
        assert!((0..100).all(|_| h.allow("192.0.2.1", t0)), "the guard was on at 0");
        assert_eq!(h.tracked(), 0, "a disabled guard tracked an address");
    }

    #[test]
    fn past_the_tracked_cap_a_newcomer_is_allowed_and_not_tracked() {
        let h = ConnectHistory::new(1, Duration::from_secs(10), Duration::from_secs(60), 2);
        let t0 = Instant::now();
        assert!(h.allow("192.0.2.1", t0) && h.allow("192.0.2.2", t0));
        assert!((0..5).all(|_| h.allow("192.0.2.3", t0)), "a newcomer past the cap was limited");
        assert_eq!(h.tracked(), 2, "a newcomer past the cap was tracked");
        assert!(!h.allow("192.0.2.1", t0), "a tracked address escaped its limit");
    }

    #[test]
    fn one_address_cannot_take_the_whole_pool() {
        let pool = Pool::new("test", 10, 2);
        let _a = pool.admit("203.0.113.7").ok().unwrap();
        let _b = pool.admit("203.0.113.7").ok().unwrap();
        assert!(pool.admit("203.0.113.7").is_err(), "a third slot for one address was admitted");
        assert!(pool.admit("198.51.100.1").is_ok(), "another address was refused by the first one's cap");
    }

    #[test]
    fn the_total_cap_holds_across_addresses() {
        let pool = Pool::new("test", 3, 2);
        let _held: Vec<_> =
            ["192.0.2.1", "192.0.2.2", "192.0.2.3"].iter().map(|ip| pool.admit(ip).ok().unwrap()).collect();
        assert!(pool.admit("192.0.2.4").is_err(), "a slot past the total cap was admitted");
    }

    #[test]
    fn a_dropped_slot_is_given_back_and_leaves_no_entry() {
        let pool = Pool::new("test", 10, 1);
        let slot = pool.admit("192.0.2.9").ok().unwrap();
        assert_eq!((pool.held(), pool.held_by("192.0.2.9"), pool.addresses()), (1, 1, 1));
        drop(slot);
        assert_eq!(
            (pool.held(), pool.held_by("192.0.2.9"), pool.addresses()),
            (0, 0, 0),
            "the slot or its address entry outlived the drop"
        );
        assert!(pool.admit("192.0.2.9").is_ok(), "the given-back slot was not admissible again");
    }

    #[test]
    fn a_v6_prefix_is_one_address() {
        // Rotating inside one /64 is free for its owner, so the cap counts the prefix.
        let pool = Pool::new("test", 10, 1);
        let _a = pool.admit("2001:db8:1:2::1").ok().unwrap();
        assert!(pool.admit("2001:db8:1:2::ffff").is_err(), "a second address in the same /64 was admitted");
        assert!(pool.admit("2001:db8:1:3::1").is_ok(), "a different /64 was refused");
    }

    #[test]
    fn refusals_are_logged_at_most_once_a_period_and_counted() {
        let pool = Pool::new("test", 1, 1);
        let t0 = Instant::now();
        assert_eq!(pool.note_refusal(t0), Some(1), "the first refusal was not logged");
        assert_eq!(pool.note_refusal(t0), None, "a second refusal inside the period was logged");
        assert_eq!(pool.note_refusal(t0), None);
        assert_eq!(pool.note_refusal(t0 + REFUSAL_LOG_EVERY), Some(3), "the line did not count the unlogged ones");
    }
}
