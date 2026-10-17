//! The master's own environment: the ports and the secrets it refuses to start without.
//! Read once, at first use, and never mutated -- so every module reads one snapshot.

use crate::common::{canon_host_name, env_int, env_str, log};
use std::sync::LazyLock;

pub struct Config {
    pub port: u16,
    pub turn_secret: String,
    pub signaling_token: String,
    pub signaling_url: String,
    pub stun_uri: String,
    pub turn_uri: String,
    /// The names a lobby proof may have been signed for: `COOP_DOMAIN` first, then each of
    /// `COOP_MASTER_NAME_ALIASES`. Empty when neither is set, and the binary refuses to start.
    pub public_names: Vec<String>,
    /// `COOP_MIN_LISTED_PROTO`: a create below it is refused. 0 (the default) refuses nothing.
    pub min_listed_proto: i64,
}

/// Whether a (canonical) name points back at the machine that dials it: `localhost`, a 127.x.x.x
/// or 0.0.0.0 literal, the IPv6 loopback. Such a name is a dev master's, never a production one's.
pub fn is_loopback_name(name: &str) -> bool {
    if name == "localhost" || name.ends_with(".localhost") || name == "0.0.0.0" {
        return true;
    }
    if matches!(name, "::1" | "[::1]") {
        return true;
    }
    let octets: Vec<&str> = name.split('.').collect();
    octets.len() == 4
        && octets.iter().all(|o| !o.is_empty() && o.len() <= 3 && o.bytes().all(|b| b.is_ascii_digit()))
        && octets[0].parse::<u16>() == Ok(127)
}

/// The names a master answers to: `domain` (canonicalised, kept whatever it is) then the comma
/// list `aliases`, each trimmed and lower-cased. An alias that is empty is skipped, a loopback
/// one is refused and named in the second list (the caller logs it), a repeat is dropped.
pub fn parse_public_names(domain: &str, aliases: &str) -> (Vec<String>, Vec<String>) {
    let mut names = Vec::new();
    let mut refused = Vec::new();
    let domain = canon_host_name(domain);
    if !domain.is_empty() {
        names.push(domain);
    }
    for alias in aliases.split(',').map(canon_host_name).filter(|a| !a.is_empty()) {
        if is_loopback_name(&alias) {
            refused.push(alias);
        } else if !names.contains(&alias) {
            names.push(alias);
        }
    }
    (names, refused)
}

/// What is wrong with the master's settings, one line each; empty when it may start. `domain` is
/// `COOP_DOMAIN` as written: it must be a bare host name, because the name a client signs is the
/// host of the URL it dialled, and a URL, a path, a login or a port written here would match no
/// client for good while the master ran and answered `/healthz`. `floor` is
/// `COOP_MIN_LISTED_PROTO` as written ("" when unset): a value that is not a decimal number would
/// otherwise read as 0 and silently lift the floor.
pub fn config_faults(domain: &str, floor: &str) -> Vec<String> {
    let mut faults = Vec::new();
    let d = domain.trim();
    if !d.is_empty() {
        let lower = d.to_ascii_lowercase();
        let bare = lower.strip_suffix('.').unwrap_or(&lower);
        if d.contains("://") || d.contains('/') || d.contains('@') || d.chars().any(char::is_whitespace) {
            faults.push(format!("COOP_DOMAIN '{d}' is not a bare host name (no scheme, path, login or spaces)"));
        } else if canon_host_name(d) != bare {
            faults.push(format!("COOP_DOMAIN '{d}' is not a bare host name (no port, plain ASCII)"));
        }
    }
    let f = floor.trim();
    if !f.is_empty() && !(f.bytes().all(|b| b.is_ascii_digit()) && f.parse::<i64>().is_ok()) {
        faults.push(format!("COOP_MIN_LISTED_PROTO '{f}' is not a decimal number"));
    }
    faults
}

impl Config {
    fn from_env() -> Config {
        let (public_names, refused) =
            parse_public_names(&env_str("COOP_DOMAIN", ""), &env_str("COOP_MASTER_NAME_ALIASES", ""));
        for alias in &refused {
            log(&format!("WARN: COOP_MASTER_NAME_ALIASES entry '{alias}' refused (a loopback name)"));
        }
        Config {
            port: env_int("COOP_MASTER_PORT", 10001) as u16,
            turn_secret: env_str("COOP_TURN_SECRET", ""),
            signaling_token: env_str("COOP_SIGNALING_TOKEN", ""),
            signaling_url: env_str("COOP_SIGNALING_URL", ""),
            stun_uri: env_str("COOP_STUN_URI", ""),
            turn_uri: env_str("COOP_TURN_URI", ""),
            public_names,
            min_listed_proto: env_int("COOP_MIN_LISTED_PROTO", 0).max(0),
        }
    }
}

pub static CFG: LazyLock<Config> = LazyLock::new(Config::from_env);

#[cfg(test)]
mod tests {
    use super::{config_faults, is_loopback_name, parse_public_names};

    #[test]
    fn loopback_names_are_recognised() {
        for n in ["localhost", "dev.localhost", "127.0.0.1", "127.1.2.3", "0.0.0.0", "::1", "[::1]"] {
            assert!(is_loopback_name(n), "{n} should be loopback");
        }
        for n in ["master.multivoid.dev", "203.0.113.7", "128.0.0.1", "1270.0.0.1", "127.0.0", "localhost.example"] {
            assert!(!is_loopback_name(n), "{n} should not be loopback");
        }
    }

    #[test]
    fn the_domain_comes_first_and_aliases_are_trimmed_and_lower_cased() {
        let (names, refused) =
            parse_public_names(" Master.Multivoid.dev ", " 203.0.113.7:10443 , Dev.Example ,, master.multivoid.dev");
        assert_eq!(names, vec!["master.multivoid.dev", "203.0.113.7", "dev.example"]);
        assert!(refused.is_empty());
    }

    #[test]
    fn a_loopback_alias_is_refused_and_named() {
        let (names, refused) = parse_public_names("master.multivoid.dev", "localhost,127.0.0.1:1,203.0.113.7");
        assert_eq!(names, vec!["master.multivoid.dev", "203.0.113.7"]);
        assert_eq!(refused, vec!["localhost", "127.0.0.1"]);
    }

    #[test]
    fn no_domain_and_no_alias_is_no_name() {
        assert!(parse_public_names("", "").0.is_empty());
        assert!(parse_public_names("  ", " , ").0.is_empty());
    }

    #[test]
    fn a_domain_must_be_a_bare_host_and_a_floor_a_decimal_number() {
        for d in ["master.multivoid.dev", " Master.Multivoid.dev ", "master.multivoid.dev.", "203.0.113.7", "[2001:db8::1]", "", "  "] {
            assert!(config_faults(d, "").is_empty(), "'{d}' was refused");
        }
        for d in [
            "https://master.multivoid.dev",
            "master.multivoid.dev/v1",
            "user@master.multivoid.dev",
            "master.multivoid.dev:10443",
            "[::1]:10443",
            "master multivoid.dev",
            "b\u{fc}cher.example",
        ] {
            assert_eq!(config_faults(d, "").len(), 1, "'{d}' was accepted");
        }
        for f in ["", " ", "0", "230", " 230 "] {
            assert!(config_faults("a.example", f).is_empty(), "floor '{f}' was refused");
        }
        for f in ["abc", "23x", "-1", "2.5", "99999999999999999999"] {
            assert_eq!(config_faults("a.example", f).len(), 1, "floor '{f}' was accepted");
        }
        assert_eq!(config_faults("https://a.example", "x").len(), 2, "both faults are named");
    }
}
