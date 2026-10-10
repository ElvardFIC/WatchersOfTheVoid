//! What one connection can make the master or the relay hold, counted on the heap by a global
//! allocator, for the pool sizes to rest on. One test function, so nothing else allocates while a
//! phase is counted. `cargo test --test conn_cost -- --nocapture` prints the table.

use serde_json::Value;
use std::alloc::{GlobalAlloc, Layout, System};
use std::sync::atomic::{AtomicIsize, Ordering};
use std::sync::Arc;
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

    // The master's parsed body, for a body at the cap and at 8 KiB.
    for cap in [64 * 1024usize, 8 * 1024] {
        for shape in ["array of zeros", "many keys", "empty objects", "one long string"] {
            let raw = body(shape, cap);
            let (v, cost) = held(|| serde_json::from_slice::<Value>(&raw).expect("the fixture is JSON"));
            println!("master body {cap} B, {shape}: {:.1} KiB parsed", kib(cost));
            drop(v);
        }
    }

    // A rustls server connection holding one partial 16 KiB handshake record.
    let cfg = Arc::new(ServerConfig::builder().with_no_client_auth().with_cert_resolver(Arc::new(NoCert)));
    let (conn, new_cost) = held(|| ServerConnection::new(cfg.clone()).expect("a server connection"));
    println!("tls server connection, new: {:.1} KiB", kib(new_cost));
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
    println!("tls server connection, a 16 KiB record pending: +{:.1} KiB", kib(buffered));
    drop(conn);
}
