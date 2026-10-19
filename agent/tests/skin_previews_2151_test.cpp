#include "support_2135.h"
#include "../../src/votv-coop/src/coop/player/skin_registry.cpp"
#include <fstream>
namespace test {std::wstring exe;}
namespace ue_wrap::paths {std::wstring ExeDir(){return test::exe;}}
namespace coop::model_folder {std::string TitleFor(const std::wstring&,const std::string& s){return s;}}
namespace coop::client_model {Wearable CanWearSkin(const std::string&){return Wearable::Yes;}}
namespace ue_wrap::game_thread {bool IsDefinitelyOffGameThread(){return false;}}
namespace ue_wrap::hot_path {uintptr_t CallerRva(const void*){return 0;}unsigned long CurrentThreadId(){return 1;}}
int main(int argc,char** argv){CHECK(argc==2);namespace fs=std::filesystem;namespace S=coop::skins;
 const fs::path root=fs::absolute(argv[1]);test::exe=(root/"Binaries"/"Win64").wstring();
 const auto logic=root/"Content"/"Paks"/"LogicMods";const auto native=logic/"multivoid",custom=logic/"Custom";
 fs::create_directories(native);fs::create_directories(custom);
 auto touch=[](const fs::path& p){std::ofstream(p).put('x');};
 touch(native/"dr_kel.png");touch(native/"kerfur_omega.bmp");touch(custom/"dr_kel.png");
 touch(native/"kerfur_mynet.png");touch(native/"kerfur_skeleton.png");
 touch(custom/"scientists.pak");touch(custom/"walter_v1sc.png");touch(custom/"user_model.pak");
 touch(custom/"user_model.bmp");touch(custom/"kerfur_omega.pak");touch(logic/"DebugMod.pak");
 const auto& list=S::Entries(true);CHECK(list.size()==31);
 auto find=[&](const std::string& n)->const S::SkinEntry& {for(const auto& e:S::Entries())if(e.name==n)return e;std::abort();};
 CHECK(fs::path(find("dr_kel").previewPath)==native/"dr_kel.png");
 CHECK(S::BuiltinSkinPath("kerfur_mynet")&&S::BuiltinSkinPath("kerfur_skeleton"));
 CHECK(fs::path(find("kerfur_mynet").previewPath)==native/"kerfur_mynet.png");
 CHECK(fs::path(find("kerfur_skeleton").previewPath)==native/"kerfur_skeleton.png");
 CHECK(fs::path(find("kerfur_omega").previewPath)==native/"kerfur_omega.bmp");
 CHECK(fs::path(find("walter_v1sc").previewPath)==custom/"walter_v1sc.png");
 CHECK(fs::path(find("user_model").previewPath)==custom/"user_model.bmp");
 CHECK(find("kerfur_alien").previewPath.empty());touch(native/"kerfur_alien.png");
 CHECK(find("kerfur_alien").previewPath.empty());S::Entries(true);
 CHECK(!find("kerfur_alien").previewPath.empty());
 const auto nested=custom/"nested"/"characters";
 fs::create_directories(nested);touch(nested/"deep_model.pak");touch(nested/"deep_model.png");
 S::Entries(true);CHECK(fs::path(find("deep_model").previewPath)==nested/"deep_model.png");
 CHECK(fs::path(find("deep_model").pakPath)==nested/"deep_model.pak");
 puts("PASS actual skin registry native/builtin/custom previews, pak dedupe, bundle and refresh");
}
