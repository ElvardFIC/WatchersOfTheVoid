// Engine access for the bay window's native canvas; no session or network state.
#include "ue_wrap/devices/window_canvas.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/gc_pin.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/world/world_singleton.h"
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

namespace ue_wrap::window_canvas {
namespace {
namespace R=reflection;
using Guard=std::function<bool()>;
uint64_t g_epoch=0, g_coreRevision=0;
bool g_drawing=false, g_creating=false;
GcPin g_brush;
CachedObjRef g_brushIdentity;
uint32_t g_brushWorld=0;
R::FName g_nOpac{},g_nCol{};

template<class T>T Read(void* object,int offset){
    T result{};std::memcpy(&result,static_cast<const uint8_t*>(object)+offset,sizeof(result));return result;
}
struct Operation {
    uint32_t world=world_identity::Generation();uint64_t epoch=g_epoch;
    bool Current()const{return world==world_identity::Generation() && epoch==g_epoch;}
};
struct Identity {
    CachedObjRef object,cls;
    explicit Identity(void* value){
        if(value && R::IsLive(value)){object.Set(value);if(object.Get())cls.Set(R::ClassOf(value));}
    }
    bool Current()const{return object.Get() && cls.Get() && R::ClassOf(object.Raw())==cls.Raw();}
};
bool Valid(const Guard& stable,const Guard& current={}){
    return stable() && (!current || current()) && stable();
}
struct Binding {
    CachedObjRef fn,owner;
    bool Live()const{return fn.Get() && owner.Get() && R::OuterOf(fn.Raw())==owner.Raw();}
};
Binding FindBinding(void* cls,const wchar_t* name,const Guard& current){
    Binding result;if(!current() || !cls || !R::IsLive(cls))return result;
    CachedObjRef source;source.Set(cls);
    void* fn=R::FindDispatchFunctionCached(cls,name);
    if(!current() || !source.Get() || !fn || !R::IsLive(fn))return result;
    result.fn.Set(fn);if(!current() || !source.Get() || !result.fn.Get())return {};
    void* owner=R::OuterOf(fn);
    if(!current() || !source.Get() || !result.fn.Get() || !owner || !R::IsLive(owner))return {};
    result.owner.Set(owner);return result.Live()?result:Binding{};
}
template<class T>bool Set(ParamFrame& frame,const wchar_t* name,const T& value,const Guard& current){
    if(!current() || !frame.valid())return false;
    const int offset=frame.ParamOffset(name);
    const int size=R::FindParamSize(frame.function(),name);
    if(!current())return false;
    const int liveOffset=R::FindParamOffset(frame.function(),name);
    if(!current() || offset<0 || liveOffset!=offset || size!=sizeof(T) || frame.FrameSize()<int(sizeof(T)) ||
        offset>frame.FrameSize()-int(sizeof(T)))return false;
    return frame.Set<T>(name,value) && current();
}
struct VectorSchema {CachedObjRef type;const wchar_t* name;void* function;};
bool Vector(ParamFrame& frame,const wchar_t* name,FVector2D value,const Guard& current,
            std::vector<VectorSchema>& schemas){
    if(!current())return false;
    void* schema=R::PropertyInnerStruct(frame.function(),name);
    if(!current() || !schema || !R::IsLive(schema))return false;
    CachedObjRef type;type.Set(schema);
    const auto valid=[&]{return current() && type.Get()==schema;};
    if(!valid() || !R::NameEquals(R::NameOf(schema),L"Vector2D") || !valid())return false;
    const int size=R::StructSize(schema);if(!valid() || size!=sizeof(FVector2D))return false;
    const int x=R::FindPropertyOffset(schema,L"X");if(!valid() || x!=offsetof(FVector2D,X))return false;
    const int y=R::FindPropertyOffset(schema,L"Y");if(!valid() || y!=offsetof(FVector2D,Y))return false;
    if(!Set(frame,name,value,valid))return false;
    schemas.push_back({type,name,frame.function()});return valid();
}
struct Core {
    CachedObjRef cls,canvasClass;
    Binding draw;
    int a=-1,canv=-1,cv=-1;uint8_t mask=0;
    bool Live()const{return cls.Get() && canvasClass.Get() && draw.Live() && a>=0 && canv>=0 && cv>=0 && mask;}
} g_core;
struct Sponge {
    CachedObjRef cls;int size=-1,strength=-1,soap=-1,mid=-1,parent=-1;
    bool Live()const{return cls.Get() && size>=0 && strength>=0 && soap>=0 && mid>=0 && parent>=0;}
} g_sponge;

bool EnsureSponge(const Guard& current){
    if(!current())return false;
    void* cls=object_index::ClassByName(L"prop_sponge_C");
    if(!current() || !cls || !R::IsLive(cls))return false;
    if(g_sponge.cls.Is(cls) && g_sponge.Live())return true;
    Sponge fields;fields.cls.Set(cls);
    const auto valid=[&]{return current() && fields.cls.Get()==cls;};
    fields.size=R::FindPropertyOffset(cls,L"size");if(!valid())return false;
    fields.strength=R::FindPropertyOffset(cls,L"strength");if(!valid())return false;
    fields.soap=R::FindPropertyOffset(cls,L"soapAmount");if(!valid())return false;
    fields.mid=R::FindPropertyOffset(cls,L"dynmat");if(!valid())return false;
    fields.parent=R::FindPropertyOffset(cls,L"drawmat");
    if(!valid() || !fields.Live() || object_index::ClassByName(L"prop_sponge_C")!=cls || !valid())return false;
    g_sponge=fields;return true;
}
bool Name(R::FName& cached,const wchar_t* text,R::FName& out,const Guard& current){
    if(!current())return false;
    if(!cached.ComparisonIndex && !cached.Number){
        const auto found=fname_utils::StringToFName(text);
        if(!current() || found.ComparisonIndex<=0)return false;
        cached=found;
    }
    out=cached;return current();
}
bool Scalar(void* material,R::FName name,float& out,const Guard& current){
    Identity target(material);
    const auto stable=[&]{return current() && target.Current();};
    if(!stable())return false;
    const auto fn=FindBinding(target.cls.Raw(),L"K2_GetScalarParameterValue",stable);
    const auto bound=[&]{return stable() && fn.Live();};
    if(!bound())return false;
    ParamFrame frame(fn.fn.Raw());if(!bound() || !Set(frame,L"ParameterName",name,bound) ||
        !Set(frame,L"ReturnValue",0.f,bound))return false;
    const bool called=Call(material,frame);if(!bound() || !called)return false;
    float value=0;if(!frame.GetRaw(L"ReturnValue",&value,sizeof(value)) || !bound() || !std::isfinite(value))return false;
    out=value;return true;
}
bool SetScalar(void* material,R::FName name,float value,const Guard& current){
    Identity target(material);
    const auto stable=[&]{return current() && target.Current();};
    if(!stable())return false;
    const auto fn=FindBinding(target.cls.Raw(),L"SetScalarParameterValue",stable);
    const auto bound=[&]{return stable() && fn.Live();};
    if(!bound())return false;
    ParamFrame frame(fn.fn.Raw());if(!bound() || !Set(frame,L"ParameterName",name,bound) ||
        !Set(frame,L"Value",value,bound))return false;
    const bool called=Call(material,frame);return bound() && called;
}

void* BrushParent(const Guard& current){
    if(!EnsureSponge(current) || !current())return nullptr;
    const auto fields=g_sponge;
    void* object=R::FindClassDefaultObject(L"prop_sponge_C");
    if(!current() || !fields.Live())return nullptr;
    if(object && R::IsLive(object)){
        Identity cdo(object);void* base=fields.cls.Raw();
        if(!cdo.Current() || !R::IsDescendantOfAny(cdo.cls.Raw(),&base,1) || !current() || !fields.Live())return nullptr;
        void* parent=Read<void*>(object,fields.parent);
        if(parent && R::IsLive(parent))return parent;
    }
    void* parent=R::FindObject(L"mat_cleanBrush_Inst",L"MaterialInstanceConstant");
    return current() && parent && R::IsLive(parent)?parent:nullptr;
}
void* EnsureBrush(void* context,const Guard& current){
    if(!current())return nullptr;
    if(g_brush.Held()){
        if(g_brushWorld==world_identity::Generation() && g_brushIdentity.Get()==g_brush.Raw())return g_brush.Raw();
        ReleaseBrush();return nullptr;
    }
    if(g_creating)return nullptr;
    struct Creating {Creating(){g_creating=true;}~Creating(){g_creating=false;}} creating;
    void* parent=BrushParent(current);if(!current() || !parent)return nullptr;
    Identity parentId(parent);
    void* cdo=R::FindClassDefaultObject(L"KismetMaterialLibrary");
    if(!current() || !parentId.Current() || !cdo || !R::IsLive(cdo))return nullptr;
    Identity library(cdo);
    const auto stable=[&]{return current() && parentId.Current() && library.Current() && !g_brush.Held();};
    const auto factory=FindBinding(library.cls.Raw(),L"CreateDynamicMaterialInstance",stable);
    const auto bound=[&]{return stable() && factory.Live();};
    if(!bound())return nullptr;
    ParamFrame frame(factory.fn.Raw());
    if(!bound() || !Set(frame,L"WorldContextObject",context,bound) ||
        !Set(frame,L"Parent",parent,bound) || !Set<void*>(frame,L"ReturnValue",nullptr,bound))return nullptr;
    const bool called=Call(cdo,frame);if(!bound() || !called)return nullptr;
    void* material=nullptr;
    if(!frame.GetRaw(L"ReturnValue",&material,sizeof(material)) || !bound() || !material || !R::IsLive(material))return nullptr;
    Identity result(material);
    void* cls=object_index::ClassByName(L"MaterialInstanceDynamic");
    if(!bound() || !result.Current() || !cls || !R::IsLive(cls))return nullptr;
    CachedObjRef type;type.Set(cls);
    if(!R::IsDescendantOfAny(result.cls.Raw(),&cls,1) || !bound() || !result.Current() || !type.Get())return nullptr;
    GcPin owned;
    if(!owned.Pin(material) || !bound() || !result.Current() || !type.Get())return nullptr;
    // Move only into the still-empty owner. A retired factory completion cannot
    // publish its pin over a brush installed by teardown/reentry.
    g_brush=std::move(owned);g_brushIdentity=result.object;g_brushWorld=world_identity::Generation();
    return material;
}
bool DabValid(const Dab& d){
    return std::isfinite(d.x) && std::isfinite(d.y) && std::isfinite(d.size) && std::isfinite(d.opac) &&
        std::isfinite(d.col) && d.x>=-512 && d.x<=2157 && d.y>=-512 && d.y<=1024 &&
        d.size>0 && d.size<=512 && d.opac>=0 && d.opac<=4 && d.col>=0 && d.col<=1;
}
}

bool EnsureResolved(){
    const Operation source;
    const auto revision=g_coreRevision;
    void* cls=object_index::ClassByName(L"d_window_C");if(!source.Current() || !cls || !R::IsLive(cls))return false;
    CachedObjRef identity;identity.Set(cls);
    void* canvas=object_index::ClassByName(L"Canvas");
    if(!source.Current() || !identity.Get() || !canvas || !R::IsLive(canvas))return false;
    if(g_core.cls.Is(cls) && g_core.canvasClass.Is(canvas) && g_core.Live())return true;
    Core fields;fields.cls=identity;fields.canvasClass.Set(canvas);
    const auto valid=[&]{return source.Current() && revision==g_coreRevision && fields.cls.Get() && fields.canvasClass.Get();};
    fields.a=R::FindPropertyOffset(cls,L"A");if(!valid())return false;
    fields.canv=R::FindPropertyOffset(cls,L"canv");if(!valid())return false;
    const bool cv=R::FindBoolProperty(cls,L"cv",fields.cv,fields.mask);if(!valid() || !cv)return false;
    fields.draw=FindBinding(canvas,L"K2_DrawMaterial",valid);
    if(!valid() || !fields.Live() || object_index::ClassByName(L"d_window_C")!=cls ||
        object_index::ClassByName(L"Canvas")!=canvas || !valid())return false;
    g_core=fields;++g_coreRevision;return true;
}
bool IsWindow(void* object){
    if(!g_core.Live())return false;Identity actor(object);
    void* base=g_core.cls.Raw();
    return actor.Current() && R::IsDescendantOfAny(actor.cls.Raw(),&base,1) && actor.Current() && g_core.Live();
}
bool IsStrokeDraw(void* object){
    if(!IsWindow(object))return false;
    return (Read<uint8_t>(object,g_core.cv)&g_core.mask)!=0;
}
bool ReadStrokeCorner(void* object,float& x,float& y){
    if(!IsWindow(object))return false;
    const auto point=Read<FVector2D>(object,g_core.a);
    if(!std::isfinite(point.X) || !std::isfinite(point.Y))return false;
    x=point.X;y=point.Y;return true;
}

bool ReadHeldBrush(void* player,float& size,float& opac,float& col,const Guard& current){
    const Operation source;Identity author(player);
    const auto valid=[&]{return Valid([&]{return source.Current() && author.Current();},current);};
    if(!valid() || !EnsureSponge(valid) || !valid())return false;
    const auto fields=g_sponge;
    const int holding=R::FindPropertyOffset(author.cls.Raw(),L"holding_actor");
    if(!valid() || !fields.Live() || holding<0)return false;
    void* held=Read<void*>(player,holding);Identity item(held);
    const auto stable=[&]{return valid() && fields.Live() && item.Current() && Read<void*>(player,holding)==held;};
    if(!stable())return false;
    void* base=fields.cls.Raw();if(!R::IsDescendantOfAny(item.cls.Raw(),&base,1) || !stable())return false;
    const float edge=Read<float>(held,fields.size);
    if(!std::isfinite(edge) || edge<=0 || edge>512)return false;
    const float strength=Read<float>(held,fields.strength),soap=Read<float>(held,fields.soap);
    // Native cleanBubbles subtracts five without clamping; a final use can leave
    // finite negative soap. getStrength treats every nonpositive value as dry.
    if(!std::isfinite(strength) || strength<0 || !std::isfinite(soap))return false;
    const auto stockCurrent=[&]{return stable() && Read<float>(held,fields.size)==edge &&
        Read<float>(held,fields.strength)==strength && Read<float>(held,fields.soap)==soap;};
    void* mid=Read<void*>(held,fields.mid);float alpha=0,color=0;
    if(mid){
        Identity material(mid);
        const auto bound=[&]{return stockCurrent() && material.Current() && Read<void*>(held,fields.mid)==mid;};
        R::FName opacity{},colour{};
        if(!bound() || !Name(g_nOpac,L"opac",opacity,bound) || !Name(g_nCol,L"col",colour,bound))return false;
        if(!Scalar(mid,opacity,alpha,bound) || !bound() || !Scalar(mid,colour,color,bound) || !bound())return false;
    }else{
        alpha=.4f*strength*(soap>0?2.f:1.f);
    }
    if(!stockCurrent() || !std::isfinite(alpha) || alpha<0 || alpha>4 || !std::isfinite(color) || color<0 || color>1)return false;
    size=edge;opac=alpha;col=color;return true;
}

bool IsSignalPlaying(){
    const Operation source;
    void* gm=world_singleton::Gamemode();if(!source.Current())return true;
    Identity gamemode(gm);
    const auto valid=[&]{return source.Current() && gamemode.Current() && world_singleton::Gamemode()==gm && source.Current() && gamemode.Current();};
    if(!valid())return true;
    const int offset=R::FindPropertyOffset(gamemode.cls.Raw(),L"analogPanels");if(!valid() || offset<0)return true;
    void* panels=Read<void*>(gm,offset);Identity target(panels);
    const auto bound=[&]{return valid() && target.Current() && Read<void*>(gm,offset)==panels;};
    if(!bound())return true;
    int byte=-1;uint8_t mask=0;
    const bool found=R::FindBoolProperty(target.cls.Raw(),L"isPlayingSignal",byte,mask);
    if(!bound() || !found || byte<0 || !mask)return true;
    return (Read<uint8_t>(panels,byte)&mask)!=0;
}

DrawResult DrawDab(void* window,const Dab& input,const Guard& current){
    const Dab d=input;const Operation source;Identity actor(window);
    const auto valid=[&]{return Valid([&]{return source.Current() && actor.Current();},current);};
    if(g_drawing || !DabValid(d) || !valid())return DrawResult::Retry;
    struct Drawing {Drawing(){g_drawing=true;}~Drawing(){g_drawing=false;}} drawing;
    if(!EnsureResolved() || !valid())return DrawResult::Retry;
    const Core fields=g_core;const auto revision=g_coreRevision;
    const auto stable=[&]{return valid() && revision==g_coreRevision && fields.Live();};
    void* base=fields.cls.Raw();
    if(!stable() || !R::IsDescendantOfAny(actor.cls.Raw(),&base,1) || !stable() || IsSignalPlaying() || !stable())return DrawResult::Retry;
    void* brush=EnsureBrush(window,stable);if(!stable() || !brush)return DrawResult::Retry;
    Identity material(brush);
    const auto owned=[&]{return stable() && material.Current() && g_brushIdentity.Get()==brush && g_brush.Raw()==brush;};
    const auto opener=FindBinding(actor.cls.Raw(),L"canvas",owned);
    // Only the audited base canvas entry is known to be repeatable preparation.
    // Inherited subclasses use it; an unverified override may already paint or
    // spend stock and must not enter the pre-draw retry path.
    const auto opening=[&]{return owned() && opener.Live() && opener.owner.Raw()==fields.cls.Raw();};
    if(!opening())return DrawResult::Retry;
    ParamFrame open(opener.fn.Raw());if(!opening() || !open.valid())return DrawResult::Retry;
    const bool opened=Call(window,open);if(!opening() || !opened)return DrawResult::Retry;
    void* canvas=Read<void*>(window,fields.canv);Identity destination(canvas);
    std::vector<VectorSchema> schemas;
    const auto bound=[&]{
        if(!owned() || !destination.Current() || Read<void*>(window,fields.canv)!=canvas ||
            !(Read<uint8_t>(window,fields.cv)&fields.mask))return false;
        for(const auto& schema:schemas)if(!schema.type.Get())return false;
        return true;
    };
    void* canvasBase=fields.canvasClass.Raw();
    if(!bound() || !R::IsDescendantOfAny(destination.cls.Raw(),&canvasBase,1) || !bound())return DrawResult::Retry;
    R::FName opacity{},colour{};
    if(!Name(g_nOpac,L"opac",opacity,bound) || !Name(g_nCol,L"col",colour,bound) ||
        !SetScalar(brush,opacity,d.opac,bound) || !bound() || !SetScalar(brush,colour,d.col,bound) || !bound())return DrawResult::Retry;
    ParamFrame frame(fields.draw.fn.Raw());
    if(!bound() || !Set(frame,L"RenderMaterial",brush,bound) ||
        !Vector(frame,L"ScreenPosition",FVector2D{d.x,d.y},bound,schemas) ||
        !Vector(frame,L"ScreenSize",FVector2D{d.size,d.size},bound,schemas) ||
        !Vector(frame,L"CoordinatePosition",FVector2D{0,0},bound,schemas) ||
        !Vector(frame,L"CoordinateSize",FVector2D{1,1},bound,schemas) ||
        !Set(frame,L"Rotation",0.f,bound) || !Vector(frame,L"PivotPoint",FVector2D{.5f,.5f},bound,schemas))return DrawResult::Retry;
    if(!bound() || IsSignalPlaying() || !bound())return DrawResult::Retry;
    for(const auto& schema:schemas){
        if(!bound())return DrawResult::Retry;
        void* live=R::PropertyInnerStruct(schema.function,schema.name);
        if(!bound() || live!=schema.type.Raw())return DrawResult::Retry;
    }
    // This is the sole draw admission. Preparation can retry; once the canvas
    // call begins, an uncertain result must not repeat a partly applied dab.
    const bool called=Call(canvas,frame);
    return called && bound()?DrawResult::Applied:DrawResult::Attempted;
}
void ReleaseBrush(){
    ++g_epoch;
    GcPin retired=std::move(g_brush);
    g_brushIdentity.Reset();g_brushWorld=0;
    // retired unroots after the live slot has been detached, preserving a new
    // owner installed by an engine teardown callback.
}
}
