"""Compile the production receive/apply/relay bodies with controlled native callbacks."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
source=Path(sys.argv[1]).read_text(encoding="utf-8-sig") if len(sys.argv)>1 else (root/"src/votv-coop/src/coop/props/container_contents_sync.cpp").read_text(encoding="utf-8-sig")
output=Path(sys.argv[2]) if len(sys.argv)>2 else root/"build/container-apply-check/container_apply_production.inc"
def body(signature):
    start=source.index(signature);begin=source.index("{",start);depth=1;end=begin+1
    while depth:
        depth+=(source[end]=="{")-(source[end]=="}");end+=1
    return source[start:end]+(";" if signature.startswith("enum ") else "")
output.parent.mkdir(parents=True,exist_ok=True)
output.write_text("\n".join(body(s) for s in [
    "void* InventoryOf(","void* OwnerOf(","bool IsWorldContainerInventory(","uint8_t* GObjStackSlot(","SliceRead ReadContents(",
    "void RelayToOthers(","void RetrySeeds(","void RederiveManagedState(","enum class Ingest",
    "Ingest ApplyContents(","Ingest ParseAndApply(","void OnContentsChunk(" ]),encoding="utf-8")
