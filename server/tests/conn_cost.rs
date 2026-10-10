//! What one connection can make the master or the relay hold, counted on the heap by a global
//! allocator, for the pool sizes to rest on. One test function, so nothing else allocates while a
//! phase is counted. `cargo test --test conn_cost -- --nocapture` prints the table.

use coop_server::admission::ConnectHistory;
use coop_server::http_transport::MAX_BODY;
use serde_json::Value;
use std::alloc::{GlobalAlloc, Layout, System};
use std::sync::atomic::{AtomicIsize, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};
use tokio_rustls::rustls::server::{ClientHello, ResolvesServerCert};
use tokio_rustls::rustls::sign::CertifiedKey;
use tokio_rustls::rustls::{ServerConfig, ServerConnection};

struct Counting;
static LIVE: AtomicIsize = AtomicIsize::new(0);

unsafe impl GlobalAlloc for Counting {
    unsafe fn alloc(&self, l: Layout) -> *mut u8 {
        let p = System.alloc(l);
        if !p.is_null() {
            LIVE.fetch_add(l.size() as isize, Ordering::SeqCst);
        }
        p
    }
    unsafe fn dealloc(&self, p: *mut u8, l: Layout) {
        System.dealloc(p, l);
        LIVE.fetch_sub(l.size() as isize, Ordering::SeqCst);
    }
    unsafe fn realloc(&self, p: *mut u8, l: Layout, n: usize) -> *mut u8 {
        let q = System.realloc(p, l, n);
        if !q.is_null() {
            LIVE.fetch_add(n as isize - l.size() as isize, Ordering::SeqCst);
        }
        q
    }
}

#[global_allocator]
static COUNTING: Counting = Counting;

/// Heap bytes `make` leaves alive while its result is held.
fn held<T>(make: impl FnOnce() -> T) -> (T, isize) {
    let before = LIVE.load(Ordering::SeqCst);
    let v = make();
    (v, LIVE.load(Ordering::SeqCst) - before)
}

#[derive(Debug)]
struct NoCert;
impl ResolvesServerCert for NoCert {
    fn resolve(&self, _: ClientHello<'_>) -> Option<Arc<CertifiedKey>> {
        None
    }
}

/// A body of `n` bytes in one of the shapes a hostile client may send.
fn body(shape: &str, n: usize) -> Vec<u8> {
    let mut b = Vec::with_capacity(n + 16);
    match shape {
        "array of zeros" => {
            b.extend_from_slice(b"{\"a\":[0");
            while b.len() + 3 < n {
                b.extend_from_slice(b",0");
            }
            b.extend_from_slice(b"]}");
        }
        "many keys" => {
            b.push(b'{');
            let mut i = 0u32;
            while b.len() + 24 < n {
                if i > 0 {
                    b.push(b',');
                }
                b.extend_from_slice(format!("\"{i:x}\":0").as_bytes());
                i += 1;
            }
            b.push(b'}');
        }
        "empty objects" => {
            b.extend_from_slice(b"{\"a\":[{}");
            while b.len() + 5 < n {
                b.extend_from_slice(b",{}");
            }
            b.extend_from_slice(b"]}");
        }
        "one long string" => {
            b.extend_from_slice(b"{\"a\":\"");
            while b.len() + 3 < n {
                b.push(b'x');
            }
            b.extend_from_slice(b"\"}");
        }
        _ => unreachable!(),
    }
    b
}

#[test]
fn connection_costs() {
    let kib = |n: isize| n as f64 / 1024.0;
    println!("phase | heap held");

    // Each row prints what it measured and fails the test past its bound, which sits about 25%
    // above today's figure: a pool size in the servers rests on these rows.
    let bound = |what: &str, cost: isize, limit_kib: f64| {
        println!("{what}: {:.1} KiB (bound {limit_kib:.1})", kib(cost));
        assert!(kib(cost) <= limit_kib, "{what} cost {:.1} KiB, past its bound of {limit_kib:.1}", kib(cost));
    };

    // The master's parsed body, at the cap it enforces.
    let cap = MAX_BODY;
    for (shape, limit) in [
        ("array of zeros", 81.0),
        ("many keys", 70.0),
        ("empty objects", 81.0),
        ("one long string", 5.8),
    ] {
        let raw = body(shape, cap);
        let (v, cost) = held(|| serde_json::from_slice::<Value>(&raw).expect("the fixture is JSON"));
        bound(&format!("master body {cap} B, {shape}, parsed"), cost, limit);
        drop(v);
    }

    // One tracked address that connected 20 times, the flood guard's per-address cost.
    let history = ConnectHistory::new(20, Duration::from_secs(10), Duration::from_secs(60), 10_000);
    let now = Instant::now();
    let ((), tracked) = held(|| {
        for _ in 0..20 {
            assert!(history.allow("203.0.113.7", now));
        }
    });
    bound("connect history, one address, 20 connects", tracked, 1.0);

    // The same per address once the table is full of them: what the tracked cap multiplies.
    const MANY: usize = 1792;
    let history = ConnectHistory::new(20, Duration::from_secs(10), Duration::from_secs(60), 10_000);
    let ((), many) = held(|| {
        for a in 0..MANY {
            let ip = format!("10.{}.{}.{}", a >> 16, (a >> 8) & 255, a & 255);
            for _ in 0..20 {
                assert!(history.allow(&ip, now));
            }
        }
    });
    bound(&format!("connect history, {MANY} addresses, 20 connects each"), many, 1320.0);

    // And at the setting's ceiling of 40 connects per address.
    let history = ConnectHistory::new(40, Duration::from_secs(10), Duration::from_secs(60), 10_000);
    let ((), many40) = held(|| {
        for a in 0..MANY {
            let ip = format!("10.{}.{}.{}", a >> 16, (a >> 8) & 255, a & 255);
            for _ in 0..40 {
                assert!(history.allow(&ip, now));
            }
        }
    });
    println!("  -> {:.0} B per address at 40 connects", many40 as f64 / MANY as f64);
    bound(&format!("connect history, {MANY} addresses, 40 connects each"), many40, 2600.0);

    // A rustls server connection holding one partial 16 KiB handshake record.
    let cfg = Arc::new(ServerConfig::builder().with_no_client_auth().with_cert_resolver(Arc::new(NoCert)));
    let (conn, new_cost) = held(|| ServerConnection::new(cfg.clone()).expect("a server connection"));
    bound("tls server connection, new", new_cost, 1.0);
    let mut conn = conn;
    let mut record = vec![0x16u8, 0x03, 0x01, 0x40, 0x00];
    record.resize(5 + 16 * 1024 - 1, 0);
    let ((), buffered) = held(|| {
        let mut rd = &record[..];
        while !rd.is_empty() {
            if conn.read_tls(&mut rd).unwrap_or(0) == 0 {
                break;
            }
        }
    });
    bound("tls server connection, a 16 KiB record pending", buffered, 40.0);
    drop(conn);
}
