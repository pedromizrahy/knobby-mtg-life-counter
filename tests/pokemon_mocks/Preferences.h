#ifndef TEST_PREFERENCES_H
#define TEST_PREFERENCES_H
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
class Preferences {
public:
 static std::map<std::string,std::vector<uint8_t>> data;
 static bool fail_next_write;
 bool active=false;
 std::string ns;
 bool begin(const char *name,bool readOnly=false) {
     (void)readOnly;
     if(!name || !*name)return false;
     ns=name;active=true;return true;
 }
 void end(){active=false;}
 std::string key(const char *k) const {return ns+":"+k;}
 bool isKey(const char *k) {return active && data.count(key(k))!=0;}
 size_t getBytesLength(const char *k) {
     if(!isKey(k)) return 0;
     return data.at(key(k)).size();
 }
 size_t getBytes(const char *k,void *out,size_t max) {
     if(!isKey(k)||!out)return 0;
     const auto &bytes=data.at(key(k));
     if(max<bytes.size())return 0;
     std::memcpy(out,bytes.data(),bytes.size());
     return bytes.size();
 }
 size_t putBytes(const char *k,const void *buf,size_t size) {
     if(!active||!buf||fail_next_write) {
         fail_next_write=false;return 0;
     }
     const uint8_t *p=(const uint8_t*)buf;
     data[key(k)]=std::vector<uint8_t>(p,p+size);
     return size;
 }
 bool remove(const char *k) {
     if(!active)return false;
     return data.erase(key(k))>0;
 }
};
#endif
