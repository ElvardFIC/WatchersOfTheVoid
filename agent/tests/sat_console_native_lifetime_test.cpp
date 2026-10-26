#define SAT_CONSOLE_INPUT_FIXTURE_ONLY
#include "sat_console_input_wrapper_test.cpp"
int main(int argc,char** argv){
 using namespace test;
 const std::string selected=argc>1?argv[1]:"";const auto run=[&](const char* name){return selected.empty() || selected==name;};
 const auto start=[] {
  Reset();nativeMode=true;metadataReady=true;frameBytes=40;
  objects[16].cls=&objects[22];objects[17].cls=&objects[23];objects[18].cls=&otherClass;
  objects[19].cls=objects[20].cls=&otherClass;
  atlasObject=&objects[16];deskObject=&objects[17];objects[16].atlasTerm=&term;
  SC::ResetLayout();SC::g_nextTryMs=0;CHECK(SC::EnsureResolved());
 };
 const auto set=[](int offset,const std::wstring& text){CHECK(ue_wrap::field_io::WriteFStringField(&term,offset,text));};
 const auto read=[](const ue_wrap::field_io::FStringView& value){return value.num>0?std::wstring(value.data,size_t(value.num-1)):std::wstring{};};
 if(run("binding")){
  start();terminalBinding=&otherClass;CHECK(termClass.live && !SC::LayoutCurrent());
  term.cls=&otherClass;CHECK(SC::EnsureResolved() && SC::LayoutCurrent());
  bool busy=false;CHECK(SC::ReadProcessing(&term,busy));
  start();writeBinding=&otherFunction;CHECK(objects[14].live && !SC::LayoutCurrent());CHECK(SC::EnsureResolved() && SC::LayoutCurrent());
  start();objects[0].cls=&otherClass;libraryBinding=&objects[0];CHECK(textLibrary.live && !SC::LayoutCurrent());CHECK(SC::EnsureResolved() && SC::LayoutCurrent());
  puts("PASS console native binding");
 }
 if(run("atlas")){
  start();CHECK(SC::LocalTerminal()==&term);
  objects[16].cls=&otherClass;CHECK(!SC::LocalTerminal());objects[16].cls=&objects[22];
  objects[16].live=false;CHECK(!SC::LocalTerminal());puts("PASS console native atlas");
 }
 if(run("factory")){
  start();CHECK(SC::CreateTerminal(&objects[18])==&otherTerm && nativeCalls==1);
  start();onFrame=[]{++world;};CHECK(!SC::CreateTerminal(&objects[18]) && !nativeCalls);
  start();onNative=[]{++world;};CHECK(!SC::CreateTerminal(&objects[18]) && nativeCalls==1 && !otherTerm.live);
  puts("PASS console native factory");
 }
 if(run("init")){
  for(int cause=0;cause<3;++cause){start();onFrame=[cause]{if(cause==0)++term.serial;if(cause==1)++objects[20].serial;if(cause==2)term.cls=&otherClass;};
   CHECK(!SC::CallInit(&term,L"root",false,&objects[20]) && !nativeCalls);}
  start();CHECK(SC::CallInit(&term,L"root",false,&objects[20]) && nativeCalls==1);puts("PASS console native init");
 }
 if(run("command")){
  start();onFree=[]{++world;};CHECK(!SC::RunCommand(&term,L"sv.ping") && !nativeCalls);
  start();onFree=[&]{set(SC::g_offCommand,L"newer");};CHECK(!SC::RunCommand(&term,L"old") && !nativeCalls && read(term.command)==L"newer");
  start();CHECK(SC::RunCommand(&term,L"sv.ping") && nativeCalls==1);puts("PASS console native command");
 }
 if(run("log")){
  start();onFrame=[]{++term.serial;};CHECK(!SC::CallWriteToLog(&term,L"hello",0) && !nativeCalls);
  start();onParams=[]{++world;};CHECK(!SC::CallWriteToLog(&term,L"hello",0) && !nativeCalls);
  start();CHECK(!SC::CallWriteToLog(&term,L"hello",255));CHECK(!SC::CallWriteToLog(&term,std::wstring(8193,L'x'),0));CHECK(!nativeCalls);
  CHECK(SC::CallWriteToLog(&term,L"hello",2) && nativeCalls==1);puts("PASS console native log");
 }
 if(run("box-class")){
  start();frameBytes=ue_wrap::ftext_utils::kFTextSize;onMint=[]{box.cls=&otherClass;};CHECK(!Consume());Untouched();
  start();frameBytes=ue_wrap::ftext_utils::kFTextSize;onMint=[]{++boxClass.serial;};CHECK(!Consume());Untouched();
  puts("PASS console native box-class");
 }
 if(run("clear")){
  start();set(SC::g_offLog,L"visible");set(offsetof(Obj,logText),L"backing");missingLogText=true;
  CHECK(!SC::ClearLog(&term) && read(term.log)==L"visible" && read(term.logText)==L"backing" && !nativeCalls);
  start();set(SC::g_offLog,L"visible");set(offsetof(Obj,logText),L"backing");onProperty=[]{++world;};
  CHECK(!SC::ClearLog(&term) && read(term.log)==L"visible" && read(term.logText)==L"backing");
  start();set(SC::g_offLog,L"visible");set(offsetof(Obj,logText),L"backing");onFree=[]{++term.serial;};
  CHECK(!SC::ClearLog(&term) && read(term.logText)==L"backing" && !nativeCalls);
  start();set(SC::g_offLog,L"visible");set(offsetof(Obj,logText),L"backing");CHECK(SC::ClearLog(&term) && read(term.log).empty() && read(term.logText).empty() && nativeCalls==1);
  puts("PASS console native clear");
 }
 if(run("arguments")){
  start();struct Args {ue_wrap::field_io::FStringView value;uint8_t type=2;} args;
  wchar_t text[]=L"hello";args.value={text,6,6};const auto* raw=reinterpret_cast<const uint8_t*>(&args);
  std::wstring output=L"unchanged";uint8_t type=9;nativeFrameSize=8;
  CHECK(!SC::ReadEnterCommandLine(raw,output) && output==L"unchanged");
  CHECK(!SC::ReadWriteToLogArgs(raw,output,type) && output==L"unchanged" && type==9);
  nativeFrameSize=40;SC::g_offWriteType=40;CHECK(!SC::ReadWriteToLogArgs(raw,output,type) && output==L"unchanged");
  SC::g_offWriteType=16;args.type=255;CHECK(!SC::ReadWriteToLogArgs(raw,output,type) && output==L"unchanged" && type==9);
  args.type=2;CHECK(SC::ReadWriteToLogArgs(raw,output,type) && output==L"hello" && type==2);
  puts("PASS console native arguments");
 }
 if(run("panels")){
  start();void* panels[]{nullptr,&objects[19]};objects[17].panels={reinterpret_cast<uint8_t*>(panels),2,2};
  CHECK(SC::FindPanelByName(L"test")==&objects[19]);objects[17].panels.max=1;CHECK(!SC::FindPanelByName(L"test"));
  objects[17].panels={reinterpret_cast<uint8_t*>(uintptr_t(1)),4097,4097};CHECK(!SC::FindPanelByName(L"test"));
  objects[17].panels={reinterpret_cast<uint8_t*>(panels),2,2};onName=[]{++world;};CHECK(!SC::FindPanelByName(L"test"));
  puts("PASS console native panels");
 }
 Reset();return 0;
}
