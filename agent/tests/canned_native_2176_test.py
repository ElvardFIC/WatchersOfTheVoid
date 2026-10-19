"""Check can variants and the held-use path against local game assets."""
from canned_native_2174_test import research, Package, body

for index in range(4):
    name = f"prop_cannedFood_{index}"
    pkg = Package(str(research / f"assets/VotV__Content__objects__{name}.uasset"))
    cls = next(e for e in pkg.exports if e["name"] == f"{name}_C")
    assert pkg.obj(cls["sup"]).endswith(".prop_cannedFood_C"), name
    functions = {e["name"] for e in pkg.exports if pkg.class_of_export(e) == "Function"}
    assert not functions.intersection({"getData", "loadData", "eat", "open"}), name

player = Package(str(research / "assets/VotV__Content__main__mainPlayer.uasset"))
graph = body(player, "ExecuteUbergraph_mainPlayer")
assert "playerHandUse_RMB(self)" in graph[38593]
assert graph[38631] == "pop_flow"  # this caller does not capture a fresh hold record after use
print("PASS four can variants inherit the record schema; held-use caller returns without recapturing it")
