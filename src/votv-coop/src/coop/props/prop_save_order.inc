// Only active native/transport scopes retain a revision entry. Unrelated keys
// never cancel one another, and disconnect leaves stack-owned scopes intact.
struct RecordOrder {unsigned scopes=0;uint64_t revision=0;};
std::map<std::wstring,RecordOrder> g_recordOrder;
uint64_t g_recordRevision=0;
struct RecordScope {
    std::wstring key;
    explicit RecordScope(const std::wstring& value):key(value){
        auto& state=g_recordOrder[key];if(!state.scopes++)state.revision=++g_recordRevision;
    }
    ~RecordScope(){auto it=g_recordOrder.find(key);if(it!=g_recordOrder.end() && !--it->second.scopes)g_recordOrder.erase(it);}
    uint64_t Revision()const{const auto it=g_recordOrder.find(key);return it==g_recordOrder.end()?0:it->second.revision;}
};
void RecordChanged(const std::wstring& key){
    const auto it=g_recordOrder.find(key);if(it!=g_recordOrder.end())it->second.revision=++g_recordRevision;
}

// Initial state stays with its author until native application, timeout or departure.
// A distinct wait revision protects repeated births with the same key and author.
constexpr size_t kMaxAwaiting=256;
constexpr uint64_t kAwaitTimeoutMs=15000;
struct Await {uint8_t senderSlot=0;uint64_t deadlineMs=0,seq=0;};
std::map<std::wstring,Await> g_awaiting;
uint64_t g_awaitSeq=0;
bool AwaitCompatible(const std::wstring& key,uint8_t sender,uint64_t sequence){
    if(!StillAwaited(key))return true;
    const auto& wait=g_awaiting.at(key);
    return sequence && wait.seq==sequence && wait.senderSlot==sender;
}
