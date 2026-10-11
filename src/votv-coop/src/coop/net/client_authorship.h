// coop/net/client_authorship.h -- which reliable kinds a client may author to the host, one row per kind.
//
// MTA lets a client ask the server to run an event only if the event was registered as remotely triggerable,
// and checks that once, where the event arrives (reference/mtasa-blue/Server/mods/deathmatch/logic/CGame.cpp:
// 2663-2710; the server's own events register false, :1668). Ours is the same flag per kind, kept beside the lane
// and relay tables because our kinds are the protocol's fixed enum rather than runtime registrations: the host's
// receive edge (session_receive.cpp) drops a Host kind from a seated client before it is inboxed or relayed.
//
// Every enumerator is named below, and .github/ci/reliablekind_gate.py fails the build when one is missing, named
// twice, relayable without being Client, or Unverified without a line in the gate's baseline list, which only
// shrinks. A value that names no kind is Host. A new kind is Host or Client from its first commit.
//   Client     -- an honest client sends it to the host (its sending path read).
//   Host       -- no client may author it: its handler refused a non-host sender by name before this table moved
//                 the refusal here, or its only senders are the host's (the save transfer, the trash re-skin, the
//                 admission challenge and seat).
//   Unverified -- accepted from a client as before, with no evidence yet either way; each moves to a row above.

#pragma once

#include "coop/net/protocol.h"  // ReliableKind

#include <cstdint>

namespace coop::net {

enum class ClientAuthorship : uint8_t { Client, Host, Unverified };

constexpr ClientAuthorship AuthorshipOf(ReliableKind k) {
    switch (k) {
    case ReliableKind::Join:
    case ReliableKind::PropRelease:
    case ReliableKind::PropSpawn:
    case ReliableKind::PropDestroy:
    case ReliableKind::LightState:
    case ReliableKind::ContainerState:
    case ReliableKind::ItemActivate:
    case ReliableKind::WindowCleanState:
    case ReliableKind::GrimeState:
    case ReliableKind::GarageDoorState:
    case ReliableKind::ApplianceState:
    case ReliableKind::PowerControlState:
    case ReliableKind::AtvState:
    case ReliableKind::OrderRequest:
    case ReliableKind::FireflySpawn:
    case ReliableKind::SaveTransferRequest:
    case ReliableKind::ClientWorldReady:
    case ReliableKind::TrashPileState:
    case ReliableKind::InventoryPickup:
    case ReliableKind::ChatMessage:
    case ReliableKind::LockerDoorState:
    case ReliableKind::DeviceClaim:
    case ReliableKind::DishAimState:
    case ReliableKind::EmailDelete:
    case ReliableKind::SavedSignalAppend:
    case ReliableKind::SavedSignalDelete:
    case ReliableKind::VoiceState:
    case ReliableKind::KerfurConvertRequest:
    case ReliableKind::PropStickState:
    case ReliableKind::DeskLogLine:
    case ReliableKind::SleepState:
    case ReliableKind::PlayerInventoryBlob:
    case ReliableKind::KerfurCommand:
    case ReliableKind::AtvRelease:
    case ReliableKind::GrabIntent:
    case ReliableKind::ThrowIntent:
    case ReliableKind::SkinChange:
    case ReliableKind::NameplateChange:
    case ReliableKind::AlarmState:
    case ReliableKind::NickColorChange:
    case ReliableKind::HandItem:
    case ReliableKind::PropDropIntent:
    case ReliableKind::RoachConsumed:
    case ReliableKind::OwnerEntitySpawn:
    case ReliableKind::OwnerEntityPose:
    case ReliableKind::OwnerEntityDestroy:
    case ReliableKind::DeskInput:
    case ReliableKind::DeskScanEvent:
    case ReliableKind::ReelSlot:
    case ReliableKind::ReelEjectIntent:
    case ReliableKind::DeskSndFx:
    case ReliableKind::LaptopState:
    case ReliableKind::PlayDeckEvent:
    case ReliableKind::PhysModsState:
    case ReliableKind::DriveSlotState:
    case ReliableKind::DrivePayload:
    case ReliableKind::RackState:
    case ReliableKind::MeadowAppend:
    case ReliableKind::MeadowDelete:
    case ReliableKind::MeadowOrder:
    case ReliableKind::LaptopQuad:
    case ReliableKind::FloppyBoxState:
    case ReliableKind::ContainerContents:
    case ReliableKind::CoinGunSell:
    case ReliableKind::CoinCollect:
    case ReliableKind::AuthHello:
    case ReliableKind::AuthProof:
    case ReliableKind::PropSaveDataIntent:
    case ReliableKind::FloppySlotState:
    case ReliableKind::HookState:
    case ReliableKind::HookDestroy:
    case ReliableKind::HookAnchorCommit:
    case ReliableKind::BroomStroke:
    case ReliableKind::LinkProbe:
    case ReliableKind::LinkProbeReply:
    case ReliableKind::PackTrashIntent:
    case ReliableKind::DroneFlyIntent:
    case ReliableKind::UpgradeIntent:
    case ReliableKind::WindowStroke:
    case ReliableKind::DoorVerbIntent:
    case ReliableKind::KeypadIntent:
    case ReliableKind::OvenRepairState:
    case ReliableKind::KerfusIntent:
    case ReliableKind::ServerUpgradeState:
    case ReliableKind::SatConsole:
    case ReliableKind::DishCalibIntent:
    case ReliableKind::PowerGridState:
    case ReliableKind::DeskVerb:
    case ReliableKind::EraserPressIntent:
    case ReliableKind::CoordTowerState:
    case ReliableKind::DeskPingVerdict:
    case ReliableKind::ServerRepair:
    case ReliableKind::CommandRequest:
    case ReliableKind::StatOrderReply:
    case ReliableKind::StatQueryReply:
    case ReliableKind::PointSackRedeem:
        return ClientAuthorship::Client;
    case ReliableKind::EntitySpawn:
    case ReliableKind::EntityDestroy:
    case ReliableKind::RestoreVitals:
    case ReliableKind::TeleportClient:
    case ReliableKind::DoorState:
    case ReliableKind::WeatherState:
    case ReliableKind::RedSky:
    case ReliableKind::LightningStrike:
    case ReliableKind::AssignPeerSlot:
    case ReliableKind::RosterRow:
    case ReliableKind::PlayerDamage:
    case ReliableKind::BalanceSync:
    case ReliableKind::KeypadState:
    case ReliableKind::SnapshotBegin:
    case ReliableKind::SnapshotComplete:
    case ReliableKind::SkyState:
    case ReliableKind::DroneState:
    case ReliableKind::PropConvert:
    case ReliableKind::SaveTransferBegin:
    case ReliableKind::SaveTransferChunk:
    case ReliableKind::TurbineState:
    case ReliableKind::SkySignalState:
    case ReliableKind::EmailAppend:
    case ReliableKind::CompState:
    case ReliableKind::CompData:
    case ReliableKind::WispGrab:
    case ReliableKind::WispTear:
    case ReliableKind::AtvSpawn:
    case ReliableKind::AtvDestroy:
    case ReliableKind::KerfurConvert:
    case ReliableKind::WorldActorSpawn:
    case ReliableKind::WorldActorDestroy:
    case ReliableKind::PropSnapPos:
    case ReliableKind::EventFire:
    case ReliableKind::PyramidGather:
    case ReliableKind::EventSnapshot:
    case ReliableKind::ServerState:
    case ReliableKind::RoachState:
    case ReliableKind::ChatLine:
    case ReliableKind::AuthChallenge:
    case ReliableKind::LightGroupState:
    case ReliableKind::PropDriveEnd:
    case ReliableKind::GrabRefused:
    case ReliableKind::JoinPhaseNote:
    case ReliableKind::UpgradeLevels:
    case ReliableKind::KerfusState:
    case ReliableKind::DishHashcodes:
    case ReliableKind::ServerSetting:
    case ReliableKind::CommandReply:
    case ReliableKind::PermissionGrants:
    case ReliableKind::CoinGunResult:
    case ReliableKind::OrderRefused:
    case ReliableKind::OrderQueue:
    case ReliableKind::StatOrder:
    case ReliableKind::StatQuery:
    case ReliableKind::PropSaveData:
        return ClientAuthorship::Host;
    case ReliableKind::SkySignalCatch:
    case ReliableKind::DeskState:
    case ReliableKind::EventCue:
    case ReliableKind::PileResyncRequest:
    case ReliableKind::DishArm:
    case ReliableKind::DishSnapshot:
    case ReliableKind::DishCalib:
    case ReliableKind::TaskNewState:
    case ReliableKind::ChatSpeaker:
    case ReliableKind::HookAnchored:
        return ClientAuthorship::Unverified;
    }
    return ClientAuthorship::Host;  // a value that names no kind
}

}  // namespace coop::net
