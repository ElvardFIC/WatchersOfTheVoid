//! The lobby announce proof. A lobby's identity IS an Ed25519 public key, so its host signs a
//! challenge the master issued, together with the master's own name, before the row may be
//! marked proved. The decision lives here, apart from the handler's I/O, so known-answer vectors
//! can watch it refuse (the shape of `registration.rs`). The nonces it signs over are issued from
//! the table at the foot of this file.

use crate::common::{hex_to_bytes, token_hex};
use std::collections::HashMap;
use std::time::{Duration, Instant};

// Domain tag of the announce proof. It differs from the signaling registration's
// (`multivoid-signaling-register-v1`) and the peer admission's (`multivoid-peer-admission-v1`) in
// its first bytes, so no choice of nonce can make a blob signed for one the blob of another: all
// three are signed with the peer's one durable key.
//
// The blob is tag || identity || nonce || master_name. The identity is exactly 68 characters and
// the nonce exactly 64, so the master name that follows cannot shift into either. The name is
// what defeats a relay through a rogue master: a host that dialled the rogue signs the rogue's
// name, which no honest master accepts.
pub const ANNOUNCE_TAG: &[u8] = b"multivoid-lobby-announce-v1";

/// The bytes an announce signs: tag || identity || nonce || master_name.
pub fn blob(ident: &str, nonce: &str, master_name: &str) -> Vec<u8> {
    let mut blob = Vec::with_capacity(ANNOUNCE_TAG.len() + ident.len() + nonce.len() + master_name.len());
    blob.extend_from_slice(ANNOUNCE_TAG);
    blob.extend_from_slice(ident.as_bytes());
    blob.extend_from_slice(nonce.as_bytes());
    blob.extend_from_slice(master_name.as_bytes());
    blob
}

/// The announce proof's whole DECISION. `ident` must already have passed `identity_shape_ok`;
/// `nonce` is the 64-hex value this master issued to the caller's address bucket, and
/// `master_name` the name the caller is taken to have dialled (lower-case, port stripped);
/// `proof_hex` is the 128-hex signature the host sent.
pub fn check_lobby_proof(
    ident: &str,
    nonce: &str,
    master_name: &str,
    proof_hex: &str,
) -> Result<(), &'static str> {
    let hex = ident.strip_prefix("gen:").ok_or("not a key identity")?;
    let pubkey = hex_to_bytes::<32>(hex).ok_or("key identity did not decode")?;
    let sig = hex_to_bytes::<64>(proof_hex).ok_or("malformed proof")?;
    ring::signature::UnparsedPublicKey::new(&ring::signature::ED25519, &pubkey)
        .verify(&blob(ident, nonce, master_name), &sig)
        .map_err(|_| "does not hold the key this identity names")
}

// ---- the challenge table ------------------------------------------------------------------

/// How long an issued nonce may be answered.
pub const NONCE_LIFE: Duration = Duration::from_secs(30);
/// Pending nonces one address bucket may hold, and the table in all.
pub const MAX_PENDING_PER_BUCKET: usize = 4;
pub const MAX_PENDING: usize = 4096;

struct Pending {
    bucket: String,
    expires: Instant,
}

/// Why a challenge was not issued.
#[derive(Debug, PartialEq, Eq)]
pub enum IssueRefusal {
    /// The caller's bucket already holds `MAX_PENDING_PER_BUCKET` live nonces.
    Bucket,
    /// The table holds `MAX_PENDING`.
    Table,
}

/// The nonces the master has issued and not yet seen answered: single use, bound to the address
/// bucket they were issued to, alive `NONCE_LIFE`. The clock is passed in, and every call sweeps
/// the expired first, so the table is bounded by its caps and by nothing that waits on a timer.
#[derive(Default)]
pub struct Challenges {
    pending: HashMap<String, Pending>,
}

impl Challenges {
    pub fn new() -> Challenges {
        Challenges::default()
    }

    fn sweep(&mut self, now: Instant) {
        self.pending.retain(|_, p| p.expires > now);
    }

    /// A fresh 64-hex nonce for `bucket`, or the cap that refuses it.
    pub fn issue(&mut self, bucket: &str, now: Instant) -> Result<String, IssueRefusal> {
        self.sweep(now);
        if self.pending.len() >= MAX_PENDING {
            return Err(IssueRefusal::Table);
        }
        if self.pending.values().filter(|p| p.bucket == bucket).count() >= MAX_PENDING_PER_BUCKET {
            return Err(IssueRefusal::Bucket);
        }
        let nonce = token_hex(32);
        self.pending.insert(nonce.clone(), Pending { bucket: bucket.to_string(), expires: now + NONCE_LIFE });
        Ok(nonce)
    }

    /// Spend `nonce`: true only for a live nonce issued to this `bucket`, and never twice. A nonce
    /// of another bucket is left alone, so a caller cannot burn what it was not issued.
    pub fn take(&mut self, nonce: &str, bucket: &str, now: Instant) -> bool {
        self.sweep(now);
        if self.pending.get(nonce).is_some_and(|p| p.bucket == bucket) {
            self.pending.remove(nonce);
            return true;
        }
        false
    }

    pub fn len(&self) -> usize {
        self.pending.len()
    }

    pub fn is_empty(&self) -> bool {
        self.pending.is_empty()
    }
}

#[cfg(test)]
mod tests {
    use super::{
        blob, check_lobby_proof, Challenges, IssueRefusal, ANNOUNCE_TAG, MAX_PENDING, MAX_PENDING_PER_BUCKET,
        NONCE_LIFE,
    };
    use crate::common::{hex_to_bytes, identity_shape_ok};
    use ring::rand::SystemRandom;
    use ring::signature::{Ed25519KeyPair, KeyPair};
    use std::time::{Duration, Instant};

    // The vector the client's selftest reads as well: one fixed seed, its identity, a nonce, a
    // master name, the blob and the signature an independent Ed25519 implementation produced.
    const VECTOR: &str = include_str!("../tests/vectors/lobby_proof.json");

    const N1: &str = "1111111111111111111111111111111111111111111111111111111111111111";
    const N2: &str = "2222222222222222222222222222222222222222222222222222222222222222";
    const NAME: &str = "master.multivoid.dev";

    struct Peer {
        ident: String,
        kp: Ed25519KeyPair,
    }

    fn ident_of(kp: &Ed25519KeyPair) -> String {
        let mut ident = String::from("gen:");
        for b in kp.public_key().as_ref() {
            ident.push_str(&format!("{b:02x}"));
        }
        ident
    }

    /// A peer built from a REAL generated keypair, so the positive arm takes the path a live
    /// client does.
    fn peer() -> Peer {
        let pkcs8 = Ed25519KeyPair::generate_pkcs8(&SystemRandom::new()).unwrap();
        let kp = Ed25519KeyPair::from_pkcs8(pkcs8.as_ref()).unwrap();
        Peer { ident: ident_of(&kp), kp }
    }

    fn hex(bytes: &[u8]) -> String {
        bytes.iter().map(|b| format!("{b:02x}")).collect()
    }

    /// Sign `as_ident || nonce || name` under the announce tag and render the wire proof.
    fn proof(p: &Peer, as_ident: &str, nonce: &str, name: &str) -> String {
        hex(p.kp.sign(&blob(as_ident, nonce, name)).as_ref())
    }

    #[test]
    fn the_shared_known_answer_vector_verifies_and_reproduces() {
        let v: serde_json::Value = serde_json::from_str(VECTOR).unwrap();
        let field = |k: &str| v[k].as_str().unwrap_or_else(|| panic!("vector has no {k}")).to_string();
        assert_eq!(field("tag").as_bytes(), ANNOUNCE_TAG);
        let (ident, nonce, name, sig) = (field("identity"), field("nonce"), field("master_name"), field("signature"));
        assert!(identity_shape_ok(&ident));
        assert_eq!(hex(&blob(&ident, &nonce, &name)), field("blob_hex"), "the blob layout moved");
        assert!(check_lobby_proof(&ident, &nonce, &name, &sig).is_ok(), "the vector's own proof was refused");
        // The seed behind it, signed here by ring, gives the same bytes: Ed25519 is deterministic.
        let seed: Vec<u8> = (0..32).map(|i| u8::from_str_radix(&field("seed")[i * 2..i * 2 + 2], 16).unwrap()).collect();
        let kp = Ed25519KeyPair::from_seed_unchecked(&seed).unwrap();
        assert_eq!(ident_of(&kp), ident);
        assert_eq!(hex(kp.sign(&blob(&ident, &nonce, &name)).as_ref()), sig);
        // Each input of the blob is load-bearing: the same proof under any other one is refused.
        assert!(check_lobby_proof(&ident, &nonce, "other.example", &sig).is_err());
        assert!(check_lobby_proof(&ident, N1, &name, &sig).is_err());
    }

    #[test]
    fn a_real_holder_proves() {
        let p = peer();
        assert!(identity_shape_ok(&p.ident), "the generated identity is not the shape the master gates on");
        assert!(check_lobby_proof(&p.ident, N1, NAME, &proof(&p, &p.ident, N1, NAME)).is_ok());
    }

    #[test]
    fn the_ways_to_not_hold_the_key_are_all_refused() {
        let victim = peer();
        let attacker = peer();
        assert_ne!(victim.ident, attacker.ident, "two peers minted one identity -- every arm proves nothing");

        // 1. SQUAT: the attacker signs a blob naming the VICTIM with its own key and lists the
        //    victim's identity.
        assert!(check_lobby_proof(&victim.ident, N1, NAME, &proof(&attacker, &victim.ident, N1, NAME)).is_err());
        // 2. REPLAY: a proof recorded for another nonce.
        assert!(check_lobby_proof(&victim.ident, N2, NAME, &proof(&victim, &victim.ident, N1, NAME)).is_err());
        // 3. ANOTHER MASTER: a proof minted for a rogue master's name verifies nowhere else.
        assert!(check_lobby_proof(&victim.ident, N1, NAME, &proof(&victim, &victim.ident, N1, "rogue.example")).is_err());
        // 4. ANOTHER KEY: the victim's valid proof offered for the attacker's identity.
        assert!(check_lobby_proof(&attacker.ident, N1, NAME, &proof(&victim, &victim.ident, N1, NAME)).is_err());
        // 5. NO PROOF, and the registration's wire form, which is not this one.
        assert!(check_lobby_proof(&victim.ident, N1, NAME, "").is_err());
        let good = proof(&victim, &victim.ident, N1, NAME);
        assert!(check_lobby_proof(&victim.ident, N1, NAME, &format!("auth {good}")).is_err());
        // 6. MALFORMED: short, long, uppercase (the alphabet is lowercase-only like the name it proves).
        assert!(check_lobby_proof(&victim.ident, N1, NAME, &good[..good.len() - 2]).is_err());
        assert!(check_lobby_proof(&victim.ident, N1, NAME, &format!("{good}00")).is_err());
        assert!(check_lobby_proof(&victim.ident, N1, NAME, &good.to_uppercase()).is_err());
        // 7. NOT A KEY IDENTITY.
        assert!(check_lobby_proof("h0123456789abcdef", N1, NAME, &good).is_err());
    }

    #[test]
    fn one_flipped_bit_is_refused() {
        // The arm that fails if verification is ever reduced to a length check.
        let p = peer();
        let mut line = proof(&p, &p.ident, N1, NAME).into_bytes();
        let last = line.len() - 1;
        line[last] = if line[last] == b'0' { b'1' } else { b'0' };
        assert!(check_lobby_proof(&p.ident, N1, NAME, &String::from_utf8(line).unwrap()).is_err());
    }

    #[test]
    fn the_tag_differs_from_the_other_two_in_its_first_bytes() {
        for other in [&b"multivoid-signaling-register-v1"[..], &b"multivoid-peer-admission-v1"[..]] {
            let at = ANNOUNCE_TAG.iter().zip(other).position(|(a, b)| a != b).expect("tags share a prefix");
            assert!(at <= 12, "the announce tag and {:?} share {at} leading bytes", String::from_utf8_lossy(other));
        }
    }

    #[test]
    fn a_nonce_is_64_hex_and_spent_once() {
        let (mut c, t0) = (Challenges::new(), Instant::now());
        let nonce = c.issue("198.51.100.1", t0).unwrap();
        assert!(hex_to_bytes::<32>(&nonce).is_some(), "not 64 lower-case hex: {nonce}");
        assert!(c.take(&nonce, "198.51.100.1", t0));
        assert!(!c.take(&nonce, "198.51.100.1", t0), "a nonce was spent twice");
        assert!(c.is_empty());
    }

    #[test]
    fn a_nonce_lives_thirty_seconds_and_no_longer() {
        let (mut c, t0) = (Challenges::new(), Instant::now());
        let early = c.issue("198.51.100.1", t0).unwrap();
        let late = c.issue("198.51.100.1", t0).unwrap();
        assert!(c.take(&early, "198.51.100.1", t0 + NONCE_LIFE - Duration::from_millis(1)));
        assert!(!c.take(&late, "198.51.100.1", t0 + NONCE_LIFE), "an expired nonce was spent");
        assert!(c.is_empty(), "an expired nonce was left in the table");
    }

    #[test]
    fn another_buckets_nonce_is_refused_and_not_burnt() {
        let (mut c, t0) = (Challenges::new(), Instant::now());
        let nonce = c.issue("198.51.100.1", t0).unwrap();
        assert!(!c.take(&nonce, "198.51.100.2", t0));
        assert!(c.take(&nonce, "198.51.100.1", t0), "the owner lost its nonce to another bucket's try");
        assert!(!c.take("not a nonce", "198.51.100.1", t0));
    }

    #[test]
    fn a_bucket_holds_four_and_expiry_frees_it() {
        let (mut c, t0) = (Challenges::new(), Instant::now());
        for _ in 0..MAX_PENDING_PER_BUCKET {
            c.issue("198.51.100.1", t0).unwrap();
        }
        assert_eq!(c.issue("198.51.100.1", t0), Err(IssueRefusal::Bucket));
        assert!(c.issue("198.51.100.2", t0).is_ok(), "one bucket's cap refused another");
        assert!(c.issue("198.51.100.1", t0 + NONCE_LIFE).is_ok(), "an expired nonce still counted");
    }

    #[test]
    fn the_table_holds_4096_and_sweeps_on_each_call() {
        let (mut c, t0) = (Challenges::new(), Instant::now());
        for i in 0..MAX_PENDING {
            c.issue(&format!("bucket-{}", i / MAX_PENDING_PER_BUCKET), t0).unwrap();
        }
        assert_eq!(c.len(), MAX_PENDING);
        assert_eq!(c.issue("fresh", t0), Err(IssueRefusal::Table));
        assert!(c.issue("fresh", t0 + NONCE_LIFE).is_ok());
        assert_eq!(c.len(), 1, "the sweep did not drop the expired");
    }
}
