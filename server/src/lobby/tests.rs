//! The lobby domain's unit tests.

use super::{build_claim_from_body, host_listed, LobbyLinks};
use crate::common::{build_claim_shape_ok, identity_shape_ok};

#[test]
fn a_lobby_row_names_its_players_links_in_one_word() {
    let l = |relayed, direct, lan| LobbyLinks { relayed, direct, lan };
    assert_eq!(l(0, 0, 0).word("p2p"), "");        // nobody measured yet
    assert_eq!(l(0, 0, 2).word("p2p"), "lan");
    assert_eq!(l(0, 1, 2).word("p2p"), "direct");
    assert_eq!(l(1, 1, 0).word("p2p"), "relay");   // any relayed player names the relay
    assert_eq!(l(0, 0, 0).word("direct"), "direct"); // a DIRECT lobby, before anyone joins
    assert_eq!(l(0, 0, 1).word("direct"), "direct");
}

#[test]
fn a_beats_links_are_clamped_to_the_lobby() {
    let body = serde_json::json!({"links": {"relayed": 9, "direct": -2, "lan": "1"}});
    assert_eq!(LobbyLinks::from_beat(&body, 4), Some(LobbyLinks { relayed: 4, direct: 0, lan: 1 }));
    assert_eq!(LobbyLinks::from_beat(&serde_json::json!({}), 4), None);
}

#[test]
fn identity_shape_accepts_a_real_rendered_key() {
    let k = format!("gen:{}", "ab01cd23".repeat(8)); // 64 hex chars
    assert_eq!(k.len(), 68);
    assert!(identity_shape_ok(&k));
}

#[test]
fn identity_shape_refuses_everything_that_would_dial_nobody() {
    let hex64 = "ab01cd23".repeat(8);
    // The NEGATIVES are the point: each of these would list a lobby that no
    // joiner can reach, which is indistinguishable from "P2P is broken".
    assert!(!identity_shape_ok(""));
    assert!(!identity_shape_ok("h0123456789abcdef"));       // the legacy mint
    assert!(!identity_shape_ok(&hex64));                    // no prefix
    assert!(!identity_shape_ok(&format!("str:{hex64}")));   // wrong type
    assert!(!identity_shape_ok(&format!("gen:{}", &hex64[..62]))); // short
    assert!(!identity_shape_ok(&format!("gen:{hex64}0")));  // odd length
    assert!(!identity_shape_ok(&format!("gen:{}", hex64.to_uppercase()))); // case
    assert!(!identity_shape_ok(&format!("gen:{} x", &hex64[..62])));       // spaced
}

#[test]
fn build_claim_shape_accepts_the_three_forms_a_host_sends() {
    let sha = "ab01cd23".repeat(8);
    let sig = "ef45ab67".repeat(16);
    assert!(build_claim_shape_ok(&sha, 0, ""));      // unsigned
    assert!(build_claim_shape_ok(&sha, 1, &sig));    // a release key
    assert!(build_claim_shape_ok(&sha, 255, &sig));  // the test key
}

#[test]
fn build_claim_shape_refuses_what_cannot_be_a_claim() {
    let sha = "ab01cd23".repeat(8);
    let sig = "ef45ab67".repeat(16);
    assert!(!build_claim_shape_ok(&sha, 256, &sig));
    assert!(!build_claim_shape_ok(&sha, -1, &sig));
    assert!(!build_claim_shape_ok(&sha, 0, &sig));              // unsigned with a signature
    assert!(!build_claim_shape_ok(&sha, 1, ""));                // signed without one
    assert!(!build_claim_shape_ok(&sha, 1, &sig[..127]));       // short signature
    assert!(!build_claim_shape_ok(&sha.to_uppercase(), 0, "")); // case
    assert!(!build_claim_shape_ok(&sha[..63], 0, ""));          // short hash
}

#[test]
fn a_host_body_keeps_a_well_formed_claim() {
    let sha = "ab01cd23".repeat(8);
    let sig = "ef45ab67".repeat(16);
    let signed = serde_json::json!({"build_sha": sha, "build_key": 7, "build_sig": sig});
    assert_eq!(build_claim_from_body(&signed), (sha.clone(), 7, sig.clone()));
    let unsigned = serde_json::json!({"build_sha": sha, "build_key": 0, "build_sig": ""});
    assert_eq!(build_claim_from_body(&unsigned), (sha.clone(), 0, String::new()));
    let sha_only = serde_json::json!({"build_sha": sha});
    assert_eq!(build_claim_from_body(&sha_only), (sha, 0, String::new()));
}

#[test]
fn a_host_body_drops_a_malformed_claim_and_a_missing_one_is_none() {
    let sha = "ab01cd23".repeat(8);
    let sig = "ef45ab67".repeat(16);
    let none = (String::new(), 0u8, String::new());
    let drops = |b: serde_json::Value| assert_eq!(build_claim_from_body(&b), none);
    drops(serde_json::json!({"build_sha": sha, "build_key": 0, "build_sig": sig}));
    drops(serde_json::json!({"build_sha": &sha[..63], "build_key": 0, "build_sig": ""}));
    drops(serde_json::json!({"build_sha": sha.to_uppercase(), "build_key": 0, "build_sig": ""}));
    drops(serde_json::json!({"name": "x"}));
}

#[test]
fn a_host_body_lists_unless_it_says_not() {
    assert!(!host_listed(&serde_json::json!({"listed": false})));
    assert!(host_listed(&serde_json::json!({"listed": true})));
    assert!(host_listed(&serde_json::json!({})));
}
