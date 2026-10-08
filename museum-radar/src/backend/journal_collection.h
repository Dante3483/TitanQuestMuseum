#pragma once
#include <string>
#include <cstring>
#include <vector>
#include <set>
#include <map>
#include <sstream>
namespace radar {
namespace detail {
struct JsonKV {
    std::string key;
    std::string val;
    bool isString;
};

inline int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}


inline void skipWs(const char*& p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\t')) ++p;
}

inline void utf8Put(std::string* out, unsigned int cp) {
    if (cp < 0x80) {
        out->push_back((char)cp);
    } else if (cp < 0x800) {
        out->push_back((char)(0xC0u | (cp >> 6)));
        out->push_back((char)(0x80u | (cp & 0x3Fu)));
    } else {
        out->push_back((char)(0xE0u | (cp >> 12)));
        out->push_back((char)(0x80u | ((cp >> 6) & 0x3Fu)));
        out->push_back((char)(0x80u | (cp & 0x3Fu)));
    }
}

inline bool jsonString(const char*& p, const char* end, std::string* out) {
    if (p >= end || *p != '"') return false;
    ++p;
    out->clear();
    while (p < end) {
        const char c = *p++;
        if (c == '"') return true;
        if (static_cast<unsigned char>(c)<0x20)return false;
        if (c != '\\') {
            out->push_back(c);
            continue;
        }
        if (p >= end) return false;
        const char e = *p++;
        switch (e) {
            case '"': out->push_back('"'); break;
            case '\\': out->push_back('\\'); break;
            case '/': out->push_back('/'); break;
            case 'b': out->push_back('\b'); break;
            case 'f': out->push_back('\f'); break;
            case 'n': out->push_back('\n'); break;
            case 'r': out->push_back('\r'); break;
            case 't': out->push_back('\t'); break;
            case 'u': {
                if (end - p < 4) return false;
                unsigned int cp = 0;
                for (int i = 0; i < 4; ++i) {
                    const int h = hexVal(p[i]);
                    if (h < 0) return false;
                    cp = cp * 16u + (unsigned int)h;
                }
                p += 4;
                utf8Put(out, cp);
                break;
            }
            default: return false;
        }
    }
    return false;
}

inline bool parseFlatObject(const char* p, const char* end, std::vector<JsonKV>* out, const char** why) {
    out->clear();
    *why = "?";
    skipWs(p, end);
    if (p >= end || *p != '{') {
        *why = "the line does not start with '{'";
        return false;
    }
    ++p;
    skipWs(p, end);
    if (p < end && *p == '}') {
        ++p;
        skipWs(p, end);
        if (p != end) {
            *why = "trailing text after '}'";
            return false;
        }
        return true;
    }
    for (;;) {
        skipWs(p, end);
        JsonKV kv;
        kv.isString = false;
        if (!jsonString(p, end, &kv.key)) {
            *why = "a key is not a quoted JSON string";
            return false;
        }
        skipWs(p, end);
        if (p >= end || *p != ':') {
            *why = "no ':' after a key";
            return false;
        }
        ++p;
        skipWs(p, end);
        if (p >= end) {
            *why = "the line ends where a value was expected";
            return false;
        }
        if (*p == '"') {
            if (!jsonString(p, end, &kv.val)) {
                *why = "an unterminated or badly escaped string value";
                return false;
            }
            kv.isString = true;
        } else if (*p == '{' || *p == '[') {
            *why = "a nested object or array - this reader is deliberately FLAT";
            return false;
        } else {
            const char* vs = p;
            while (p < end && *p != ',' && *p != '}') ++p;
            const char* ve = p;
            while (ve > vs && (ve[-1] == ' ' || ve[-1] == '\t')) --ve;
            if (ve == vs) {
                *why = "an empty value";
                return false;
            }
            kv.val.assign(vs, (size_t)(ve - vs));
        }
        out->push_back(kv);
        skipWs(p, end);
        if (p < end && *p == ',') {
            ++p;
            continue;
        }
        if (p < end && *p == '}') {
            ++p;
            skipWs(p, end);
            if (p != end) {
                *why = "trailing text after '}'";
                return false;
            }
            return true;
        }
        *why = "expected ',' or '}'";
        return false;
    }
}

inline bool jsonU32(const std::string& v, unsigned int* out) {
    if (v.empty() || v.size() > 10) return false;
    unsigned long long x = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] < '0' || v[i] > '9') return false;
        x = x * 10ull + (unsigned long long)(v[i] - '0');
        if (x > 0xFFFFFFFFull) return false;
    }
    *out = (unsigned int)x;
    return true;
}

}

inline std::string normalized(std::string value){
    for(char& c:value){if(c=='\\')c='/';if(c>='A'&&c<='Z')c+=32;}return value;
}
inline bool collectedJournal(const std::string& data,std::set<std::string>& result){
    result.clear();std::set<std::string> next;std::istringstream input(data);std::string line;
    bool header=false;unsigned expected=0,rows=0,collected=0,pin=0,pout=0;
    unsigned expectedCollected=0,expectedIn=0,expectedOut=0;
    while(std::getline(input,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(!header&&line.compare(0,3,"\xef\xbb\xbf")==0)line.erase(0,3);
        if(line.find_first_not_of(" \t")==std::string::npos)continue;
        std::vector<detail::JsonKV> values;const char* why=nullptr;
        if(!detail::parseFlatObject(line.data(),line.data()+line.size(),&values,&why))return false;
        std::map<std::string,detail::JsonKV> fields;
        for(const auto& value:values)if(!fields.emplace(value.key,value).second)return false;
        auto string=[&](const char* key,std::string& out){auto it=fields.find(key);
            if(it==fields.end()||!it->second.isString)return false;out=it->second.val;return true;};
        auto number=[&](const char* key,unsigned& out){auto it=fields.find(key);
            return it!=fields.end()&&!it->second.isString&&detail::jsonU32(it->second.val,&out);};
        if(!header){std::string name,set;unsigned format=0;
            if(!string("journal",name)||name!="titan quest uniquetab"||!number("format",format)||format!=1||
               !string("set",set)||set!="tq-uniq-items"||!number("entries",expected)||!number("collected",expectedCollected)||
               !number("pendingIn",expectedIn)||!number("pendingOut",expectedOut))return false;
            header=true;continue;
        }
        std::string record,base,pending;
        if(!string("record",record)||!string("base",base)||normalized(base)!=normalized(record)||
           record.size()<12||normalized(record).compare(0,8,"records/")||record.compare(record.size()-4,4,".dbr"))return false;
        for(const char* key:{"prefix","suffix","relic","relicBonus","relic2","relicBonus2"}){
            std::string value;if(!string(key,value)||value.size()>=256)return false;
            for(unsigned char c:value)if(c<0x20||c>0x7e)return false;
        }
        for(const char* key:{"seed","var1","var2","stack","b8"}){
            unsigned value=0;if(!number(key,value)||(!std::strcmp(key,"b8")&&value>255))return false;
        }
        if(fields.count("pending")&&(!string("pending",pending)||(pending!="in"&&pending!="out")))return false;
        ++rows;if(pending=="out")++pout;else {++collected;next.insert(normalized(record));if(pending=="in")++pin;}
    }
    if(!header||rows!=expected||collected!=expectedCollected||pin!=expectedIn||pout!=expectedOut||next.size()>4096)return false;
    result.swap(next);return true;
}
} // namespace radar
