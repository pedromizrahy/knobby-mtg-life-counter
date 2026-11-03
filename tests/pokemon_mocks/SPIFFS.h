#ifndef PK_TEST_SPIFFS_H
#define PK_TEST_SPIFFS_H
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#define FILE_READ "r"
#define FILE_WRITE "w"
struct MockSPIFFS;
class File {
public:
    File()=default;
    File(MockSPIFFS *fs,const std::string &name,bool writable,bool present);
    explicit operator bool() const {return valid;}
    size_t size() const;
    int read(uint8_t *dst,size_t n);
    size_t write(const uint8_t *src,size_t n);
    void flush(){}
    void close(){valid=false;}
private:
    MockSPIFFS *fs=nullptr;
    std::string path;
    size_t offset=0;
    bool writable=false,valid=false;
};
struct MockSPIFFS {
    std::map<std::string,std::vector<uint8_t>> files;
    bool mount_ok=true,fail_write=false;
    bool begin(bool format=false){return mount_ok && !format;}
    bool exists(const char *path){return files.count(path)!=0;}
    File open(const char *path,const char *mode){
        if(!mount_ok)return File();
        bool write=std::strcmp(mode,"w")==0;
        if(!write && !exists(path))return File();
        if(write)files[path]={};
        return File(this,path,write,true);
    }
};
inline File::File(MockSPIFFS *f,const std::string &n,bool w,bool v):
  fs(f),path(n),writable(w),valid(v){}
inline size_t File::size() const{return fs->files.at(path).size();}
inline int File::read(uint8_t *dst,size_t n){
    if(!valid || writable)return 0;
    const auto &b=fs->files.at(path);
    size_t count=(offset+n>b.size())?b.size()-offset:n;
    if(count)std::memcpy(dst,b.data()+offset,count);
    offset+=count;return (int)count;
}
inline size_t File::write(const uint8_t *src,size_t n){
    if(!valid || !writable || fs->fail_write)return 0;
    auto &b=fs->files[path];b.insert(b.end(),src,src+n);
    return n;
}
extern MockSPIFFS SPIFFS;
#endif
