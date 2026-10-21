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
