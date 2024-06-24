/*
 * Copyright 2023 wtcat 
 */
#include <windows.h>
#include <dbghelp.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <mutex>

#include "basework/tools/tracer/path_tracer.h"

#pragma comment (lib,"imagehlp.lib")

namespace tools {

class cc_pathtracer {
public:
    enum {
        BACKTRACE_MAX_LIMIT = 64
    };
    struct cc_pathnode {
        void* ip[BACKTRACE_MAX_LIMIT];
        size_t ip_size;
        void* ptr;
        size_t size;
        void *user;
    };
    struct cc_heapnode {
        std::string name;
        void* start;
        size_t size;
        std::vector<cc_pathnode *> vec;
        void (*print)(void *fout, const void *);
    };
    cc_pathtracer(int min_limit, int max_limit, const std::string &sep) :
        min_limit_(min_limit),
        max_limit_(max_limit),
        sep_(sep) {
    }
    cc_pathtracer() :
        min_limit_(1),
        max_limit_(60),
        sep_("/") {
    }
    ~cc_pathtracer() = default;
    cc_pathtracer(const cc_pathtracer&) = delete;
    cc_pathtracer& operator=(const cc_pathtracer&) = delete;

    void add_heap(const std::string& str, void* start, size_t size,
        void (*print)(void *fout, const void *));
    bool add(void* addr, size_t size, void *user_data);
    bool del(void* addr);
    cc_pathnode *find(void *addr);
    void dump(std::ostream& fd);
    void set_minlimit(int min_limit) {
        min_limit_ = min_limit;
    }
    void set_maxlimit(int max_limit) {
        max_limit_ = max_limit;
    }
    void set_sep(const std::string& s) {
        sep_ = s;
    }
    void set_memcheck_hook(const memcheck_hook_t hook) {
        memchk_hook_ = hook;
    }

private:
    void transform_node(cc_pathnode*, void *, std::string *);
    cc_heapnode* heap_match(void* start);

private:
    std::map<void *, cc_pathnode> container_;
    int min_limit_;
    int max_limit_;
    std::string sep_;
    std::vector<cc_heapnode> heap_;
    std::mutex mtx_;

    static memcheck_hook_t memchk_hook_;
};

memcheck_hook_t cc_pathtracer::memchk_hook_;

cc_pathtracer::cc_heapnode* cc_pathtracer::heap_match(void* start) {
    for (auto& iter : heap_) {
        if ((char*)start >= (char*)iter.start &&
            (char*)start < (char*)iter.start + iter.size)
            return &iter;
    }
    return nullptr;
}

void cc_pathtracer::add_heap(const std::string& str,
    void* start, size_t size,
    void (*print)(void *fout, const void *)) {
    cc_heapnode heap;
    heap.name = str;
    heap.start = start;
    heap.size = size;
    heap.print = print;
    std::lock_guard<std::mutex> lock(mtx_);
    heap_.push_back(heap);
}

bool cc_pathtracer::add(void* addr, size_t size, void *user_data) {
    void* ip_array[BACKTRACE_MAX_LIMIT];
    if (!addr)
        return false;
    std::lock_guard<std::mutex> lock(mtx_);
    size_t ret = CaptureStackBackTrace(min_limit_, max_limit_, ip_array, NULL);
    if (ret) {
        cc_pathnode node;
        memcpy(node.ip, ip_array, sizeof(void*) * ret);
        node.ip_size = ret;
        node.ptr = addr;
        node.size = size;
        node.user = user_data;
        container_.insert(std::make_pair(addr, node));
        return true;
    }
    return false;
}

bool cc_pathtracer::del(void* addr) {
    if (!addr)
        return false;
    std::lock_guard<std::mutex> lock(mtx_);
    auto iter = container_.find(addr);
    if (iter != container_.end()) {
        container_.erase(addr);
        return true;
    }
    return false;
}

cc_pathtracer::cc_pathnode *
cc_pathtracer::find(void *addr) {
    if (addr) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto iter = container_.find(addr);
        if (iter != container_.end())
            return &iter->second;
    }
    return nullptr;
}

void cc_pathtracer::dump(std::ostream& fd) {
    HANDLE hproc = GetCurrentProcess();
    std::map<std::string, cc_heapnode *> heap_container;
    std::string path;
    std::lock_guard<std::mutex> lock(mtx_);

    //Preppare
    for (auto& iter : container_) {
        cc_pathnode* node = &iter.second;
        cc_heapnode* heap = heap_match(node->ptr);
        if (heap != nullptr) {
            auto iter = heap_container.find(heap->name);
            if (iter == heap_container.end()) 
                heap_container.insert(std::make_pair(heap->name, heap));
            heap->vec.push_back(node);
        }
    }

    auto print = [&](cc_pathnode* node, const std::string &path,
        const cc_heapnode* heap) -> void {
        fd << "** (" << heap->name << ") ";
        if (heap->print)
            heap->print(&fd, node->user);
        fd << " Memory" << "<0x" << std::hex << node->ptr << ", " << std::dec << node->size << ">\n";
        fd << "  " << path << "\n\n";
    };

    SymInitialize(hproc, NULL, TRUE);
    fd << "******************************************************\n";
    fd << "                   MemTracer Dump                     \n";
    fd << "******************************************************\n";
    for (auto &iviter : heap_container) {
        size_t sum = 0;
        fd << "\n*************" << iviter.second->name << "*************\n\n";
        for (auto node : iviter.second->vec) {
            path.clear();
            transform_node(node, (void*)hproc, &path);
            print(node, path, iviter.second);
            if (memchk_hook_)
                memchk_hook_(path.c_str(), node->ptr, node->size);
            sum += node->size;
        }

        fd << "******" << iviter.second->name << "Size: " << std::dec << sum << " B, ";
        fd << std::defaultfloat << (float)sum / 1024 << " KB, " << (float)sum / (1024 * 1024) << " MB";
        fd << " ******\n\n";

        //Clear container
        iviter.second->vec.clear();
    }
    heap_container.clear();
}

void cc_pathtracer::transform_node(cc_pathnode *node, void *ctx,std::string *out) {
    unsigned long buffer[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO* symbol;
    
    symbol = (SYMBOL_INFO*)buffer;
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    for (int i = (int)node->ip_size - 1; i >= 0; i--) {
        void* addr = node->ip[i];
        if (addr == nullptr)
            continue;
        SymFromAddr((HANDLE)ctx, (DWORD64)addr, 0, symbol);
        symbol->Name[symbol->NameLen] = '\0';
        out->append(symbol->Name);
        out->append(sep_);
    }
}

} //namespace tools


//C-Like API
static tools::cc_pathtracer _tracer;

extern "C" void mempath_init(int min_limit, int max_limit, const char *sep) {
    static bool inited;
    if (!inited) {
        inited = true;
        if (min_limit < max_limit) {
            _tracer.set_minlimit(min_limit);
            _tracer.set_maxlimit(max_limit - min_limit);
        }
        if (sep)
            _tracer.set_sep(sep);
    }
}

extern "C" void mempath_add_heap(const char* heap, void* addr, size_t size,
    void (*print)(void *fout, const void *)) {
    _tracer.add_heap(heap, addr, size, print);
}

extern "C" bool mempath_add(void* addr, size_t size, void *user) {
    return _tracer.add(addr, size, user);
}

extern "C" size_t mempath_getsize(void* addr) {
    const tools::cc_pathtracer::cc_pathnode *node = _tracer.find(addr);
    if (node)
        return node->size;
    return 0;
}

extern "C" bool mempath_del(void* addr) {
    return _tracer.del(addr);
}

extern "C" void mempath_dump_std(void) {
    return _tracer.dump(std::cout);
}

extern "C" void mempath_dump_file(const char *filename) {
    if (!filename)
        return;
    std::ofstream fd(filename, std::ios::out | std::ios::trunc);
    if (fd.is_open()) {
        _tracer.dump(fd);
        fd.close();
    }
}

extern "C" void mempath_set_memcheck_hook(const memcheck_hook_t hook) {
    _tracer.set_memcheck_hook(hook);
}
