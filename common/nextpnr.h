/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2018  Clifford Wolf <clifford@symbioticeda.com>
 *  Copyright (C) 2018  Serge Bazanski <q3k@symbioticeda.com>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <algorithm>
#include <assert.h>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef NEXTPNR_H
#define NEXTPNR_H

#ifdef NEXTPNR_NAMESPACE
#define NEXTPNR_NAMESPACE_PREFIX NEXTPNR_NAMESPACE::
#define NEXTPNR_NAMESPACE_BEGIN namespace NEXTPNR_NAMESPACE {
#define NEXTPNR_NAMESPACE_END }
#define USING_NEXTPNR_NAMESPACE using namespace NEXTPNR_NAMESPACE;
#else
#define NEXTPNR_NAMESPACE_PREFIX
#define NEXTPNR_NAMESPACE_BEGIN
#define NEXTPNR_NAMESPACE_END
#define USING_NEXTPNR_NAMESPACE
#endif

#if defined(__GNUC__) || defined(__clang__)
#define NPNR_ATTRIBUTE(...) __attribute__((__VA_ARGS__))
#define NPNR_NORETURN __attribute__((noreturn))
#define NPNR_DEPRECATED __attribute__((deprecated))
#define NPNR_PACKED_STRUCT(...) __VA_ARGS__ __attribute__((packed))
#elif defined(_MSC_VER)
#define NPNR_ATTRIBUTE(...)
#define NPNR_NORETURN __declspec(noreturn)
#define NPNR_DEPRECATED __declspec(deprecated)
#define NPNR_PACKED_STRUCT(...) __pragma(pack(push, 1)) __VA_ARGS__ __pragma(pack(pop))
#else
#define NPNR_ATTRIBUTE(...)
#define NPNR_NORETURN
#define NPNR_DEPRECATED
#define NPNR_PACKED_STRUCT(...) __VA_ARGS__
#endif

NEXTPNR_NAMESPACE_BEGIN

class assertion_failure : public std::runtime_error
{
  public:
    assertion_failure(std::string msg, std::string expr_str, std::string filename, int line);

    std::string msg;
    std::string expr_str;
    std::string filename;
    int line;
};

inline void except_assert_impl(bool expr, const char *message, const char *expr_str, const char *filename, int line)
{
    if (!expr)
        throw assertion_failure(message, expr_str, filename, line);
}

NPNR_NORETURN
inline void assert_false_impl(std::string message, std::string filename, int line)
{
    throw assertion_failure(message, "false", filename, line);
}

#define NPNR_ASSERT(cond) except_assert_impl((cond), #cond, #cond, __FILE__, __LINE__)
#define NPNR_ASSERT_MSG(cond, msg) except_assert_impl((cond), msg, #cond, __FILE__, __LINE__)
#define NPNR_ASSERT_FALSE(msg) assert_false_impl(msg, __FILE__, __LINE__)

struct IdStringDB;
struct Context;

struct IdString
{
    int index = 0;

    static void initialize_arch(const IdStringDB *ctx);

    static void initialize_add(const IdStringDB *ctx, const char *s, int idx);

    IdString() {}

    void set(const IdStringDB *ctx, const std::string &s);

    IdString(const IdStringDB *ctx, const std::string &s) { set(ctx, s); }

    IdString(const IdStringDB *ctx, const char *s) { set(ctx, s); }

    const std::string &str(const IdStringDB *ctx) const;

    const char *c_str(const IdStringDB *ctx) const;

    bool operator<(const IdString &other) const { return index < other.index; }

    bool operator==(const IdString &other) const { return index == other.index; }

    bool operator!=(const IdString &other) const { return index != other.index; }

    bool empty() const { return index == 0; }
};

NEXTPNR_NAMESPACE_END

namespace std {
template <> struct hash<NEXTPNR_NAMESPACE_PREFIX IdString>
{
    std::size_t operator()(const NEXTPNR_NAMESPACE_PREFIX IdString &obj) const noexcept
    {
        return std::hash<int>()(obj.index);
    }
};
} // namespace std

NEXTPNR_NAMESPACE_BEGIN

struct GraphicElement
{
    enum type_t
    {
        G_NONE,
        G_LINE,
        G_BOX,
        G_CIRCLE,
        G_LABEL
    } type = G_NONE;

    enum style_t
    {
        G_FRAME,
        G_HIDDEN,
        G_INACTIVE,
        G_ACTIVE,
    } style = G_FRAME;

    float x1 = 0, y1 = 0, x2 = 0, y2 = 0, z = 0;
    std::string text;
};

NEXTPNR_NAMESPACE_END

#include "archdefs.h"

NEXTPNR_NAMESPACE_BEGIN

struct DecalXY
{
    DecalId decal;
    float x = 0, y = 0;
};

struct BelPin
{
    BelId bel;
    PortPin pin;
};

struct CellInfo;

enum PlaceStrength
{
    STRENGTH_NONE = 0,
    STRENGTH_WEAK = 1,
    STRENGTH_STRONG = 2,
    STRENGTH_FIXED = 3,
    STRENGTH_LOCKED = 4,
    STRENGTH_USER = 5
};

struct PortRef
{
    CellInfo *cell = nullptr;
    IdString port;
    delay_t budget = 0;
};

struct PipMap
{
    PipId pip = PipId();
    PlaceStrength strength = STRENGTH_NONE;
};

struct NetInfo
{
    IdString name;
    PortRef driver;
    std::vector<PortRef> users;
    std::unordered_map<IdString, std::string> attrs;

    // wire -> uphill_pip
    std::unordered_map<WireId, PipMap> wires;
};

enum PortType
{
    PORT_IN = 0,
    PORT_OUT = 1,
    PORT_INOUT = 2
};

struct PortInfo
{
    IdString name;
    NetInfo *net;
    PortType type;
};

struct CellInfo
{
    IdString name, type;
    std::unordered_map<IdString, PortInfo> ports;
    std::unordered_map<IdString, std::string> attrs, params;

    BelId bel;
    PlaceStrength belStrength = STRENGTH_NONE;

    // cell_port -> bel_pin
    std::unordered_map<IdString, IdString> pins;
};

class IdStringDB
{
    friend class IdString;
  private:
    mutable std::unordered_map<std::string, int> *idstring_str_to_idx;
    mutable std::vector<const std::string *> *idstring_idx_to_str;
  
  public:
    IdString id(const std::string &s) const { return IdString(this, s); }
    IdString id(const char *s) const { return IdString(this, s); }

    IdStringDB()
    {
        idstring_str_to_idx = new std::unordered_map<std::string, int>;
        idstring_idx_to_str = new std::vector<const std::string *>;
        IdString::initialize_add(this, "", 0);
        IdString::initialize_arch(this);
    }

    ~IdStringDB()
    {
        delete idstring_str_to_idx;
        delete idstring_idx_to_str;
    }
};

class DeterministicRNG
{
  private:
    uint64_t rngstate;

  public:
    DeterministicRNG() : rngstate(0x3141592653589793)
    {
    }

    uint64_t rng64()
    {
        // xorshift64star
        // https://arxiv.org/abs/1402.6246

        uint64_t retval = rngstate * 0x2545F4914F6CDD1D;

        rngstate ^= rngstate >> 12;
        rngstate ^= rngstate << 25;
        rngstate ^= rngstate >> 27;

        return retval;
    }

    int rng() { return rng64() & 0x3fffffff; }

    int rng(int n)
    {
        assert(n > 0);

        // round up to power of 2
        int m = n - 1;
        m |= (m >> 1);
        m |= (m >> 2);
        m |= (m >> 4);
        m |= (m >> 8);
        m |= (m >> 16);
        m += 1;

        while (1) {
            int x = rng64() & (m - 1);
            if (x < n)
                return x;
        }
    }

    void rngseed(uint64_t seed)
    {
        rngstate = seed ? seed : 0x3141592653589793;
        for (int i = 0; i < 5; i++)
            rng64();
    }

    template <typename T> void shuffle(std::vector<T> &a)
    {
        for (size_t i = 0; i != a.size(); i++) {
            size_t j = i + rng(a.size() - i);
            if (j > i)
                std::swap(a[i], a[j]);
        }
    }

    template <typename T> void sorted_shuffle(std::vector<T> &a)
    {
        std::sort(a.begin(), a.end());
        shuffle(a);
    }

};

class BaseCtx : public IdStringDB, public DeterministicRNG
{
  private:
    std::mutex mutex;
    bool mutex_owned;
    pthread_t mutex_owner;

  public:
    std::unordered_map<IdString, std::unique_ptr<NetInfo>> nets;
    std::unordered_map<IdString, std::unique_ptr<CellInfo>> cells;

    BaseCtx() {}
    ~BaseCtx() {}

    void lock(void)
    {
        mutex.lock();
        mutex_owner = pthread_self();
    }

    void unlock(void)
    {
        NPNR_ASSERT(pthread_equal(pthread_self(), mutex_owner) != 0);
        mutex.unlock();
    }

    Context *getCtx() { return reinterpret_cast<Context *>(this); }

    const Context *getCtx() const { return reinterpret_cast<const Context *>(this); }

    // --------------------------------------------------------------

    bool allUiReload = true;
    bool frameUiReload = false;
    std::unordered_set<BelId> belUiReload;
    std::unordered_set<WireId> wireUiReload;
    std::unordered_set<PipId> pipUiReload;
    std::unordered_set<GroupId> groupUiReload;

    void refreshUi() { allUiReload = true; }

    void refreshUiFrame() { frameUiReload = true; }

    void refreshUiBel(BelId bel) { belUiReload.insert(bel); }

    void refreshUiWire(WireId wire) { wireUiReload.insert(wire); }

    void refreshUiPip(PipId pip) { pipUiReload.insert(pip); }

    void refreshUiGroup(GroupId group) { groupUiReload.insert(group); }
};

NEXTPNR_NAMESPACE_END

#include "arch.h"

NEXTPNR_NAMESPACE_BEGIN

struct Context : Arch
{
    bool verbose = false;
    bool debug = false;
    bool force = false;
    bool timing_driven = true;
    float target_freq = 12e6;

    Context(ArchArgs args) : Arch(args) {}

    // --------------------------------------------------------------

    // provided by router1.cc
    bool getActualRouteDelay(WireId src_wire, WireId dst_wire, delay_t &delay);

    // --------------------------------------------------------------

    uint32_t checksum() const;

    void check() const;
};

NEXTPNR_NAMESPACE_END

#endif
