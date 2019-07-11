/*******************************************************************************
 * Copyright (c) 2019 Craig Jacobson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ******************************************************************************/

/**
 * @file hashmap.hpp
 * @author Craig Jacobson
 * @brief Hash map implementation.
 */

#ifndef CRJ_HASH_MAP_H
#define CRJ_HASH_MAP_H

#include <cstring>
#include <functional>
#include <iostream>
#include <limits>

#include <emmintrin.h>


#ifdef __GNUC__
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif

#ifdef __GNUC__
#define INLINE __attribute__((always_inline))
#define NOINLINE __attribute__((noinline))
#else
#define INLINE 
#define NOINLINE
#endif

#if SIZE_MAX == UINT32_MAX
#   define HASH_SIZE 32
#elif SIZE_MAX == UINT64_MAX
#   define HASH_SIZE 64
#else
#   error Unsupported size_t type.
#endif

#define UNUSED(x)

namespace crj
{
using size_type = std::size_t;

template <typename Key, typename Hash = std::hash<Key>>
struct FibonacciHash: public Hash
{
    FibonacciHash(const Hash& h = Hash{}) : Hash(h) {}
#pragma message (SIZE_MAX)
#if HASH_SIZE == 32
    /*
     * (2**32)/(Golden Ratio) ~= 2654435769
     * The two closest primes are 2654435761 and 2654435789
     */
    static constexpr size_type FIB = size_type(2654435761);
    static constexpr int SHIFT = 16;
#elif HASH_SIZE == 64
    /*
     * (2**64)/(Golden Ratio) ~= 11400714819323198486
     * Seems impractical to find nearest prime number, so let's make it odd.
     * 11400714819323198487
     */
    static constexpr size_type FIB = size_type(11400714819323198487ULL);
    static constexpr int SHIFT = 32;
#else
    // Error already reported above.
#endif

    size_type
    operator()(const Key& k)
    const
    {
        size_type result = FIB * Hash::operator()(k);
        return (result >> SHIFT) | (result << SHIFT);
    }
};

namespace detail
{

static constexpr int BLOCK_LEN = int(16);
static constexpr int LEAP_MAX = 1 << 6;
static constexpr int SEARCH_MASK = 0x0000FFFF;
static constexpr int HASH_BITS = 6;


struct BlockFull {};

class SearchMap
{
public:
    SearchMap(int map)
        : mMap(map)
    {}

    SearchMap(SearchMap& m)
        : mMap(m.mMap)
    {}

    SearchMap(SearchMap&& m)
        : mMap(m.mMap)
    {}

    SearchMap&
    operator=(SearchMap& m)
    {
        mMap = m.mMap;
        return *this;
    }

    SearchMap&
    operator=(SearchMap&& m)
    {
        mMap = m.mMap;
        return *this;
    }

    bool
    has()
    const noexcept
    {
        return !!mMap;
    }

    int
    next()
    const noexcept
    {
        return __builtin_ffs(mMap) - 1;
    }

    void
    clear(int i)
    {
        mMap &= ~(1 << i);
    }

    int
    value()
    const
    {
        return mMap;
    }

private:
    int mMap;
};

/**
 * @brief Define block type.
 *
 * Notes:
 * The bitwise format for the hash byte:
 * | 1           | 1        | 6         |
 * | special bit | link bit | hash bits |
 * FF = empty
 * FE = unsearchable, no search will match this entry
 * 40 = mask for the link bit
 * 3F = mask for the hash bits
 *
 * The leap byte specifies where the next entry is:
 * 0 = end of linked list
 * [1, FE] = jump distance to next link
 * FF = do inefficient search
 */
template <typename Value>
class Block
{
private:
    uint8_t mHash[BLOCK_LEN];
    uint8_t mLeap[BLOCK_LEN];
    Value   mValue[BLOCK_LEN];

    /* Hash related */
    static constexpr uint8_t EMPTY     = uint8_t(0xFF);
    static constexpr uint8_t NOFIND    = uint8_t(0xFE);
    static constexpr uint8_t LINK      = uint8_t(0x40);
    static constexpr uint8_t HASH_MASK = uint8_t(0x3F);
    /* Link related */
    static constexpr uint8_t FIND      = uint8_t(0xFF);

public:
    static Block*
    get(Block* b, size_type i)
    { return b +  (i / BLOCK_LEN); }

    static uint8_t
    set_link_hash(uint8_t h)
    { return h | LINK; }

    static uint8_t
    clear_link_hash(uint8_t h)
    { return h & HASH_MASK; }

    static bool
    can_leap(size_type d)
    { return d < size_type(FIND); }

    static void
    set_empty_all(unsigned char *p, size_type len)
    { std::memset(p, EMPTY, len); }

    static size_type
    construct_index(size_type i, int sub)
    { return (i & ~(BLOCK_LEN - 1)) + sub; }

    Block()
        : mHash{ 0xFF, 0xFF, 0xFF, 0xFF,
                 0xFF, 0xFF, 0xFF, 0xFF,
                 0xFF, 0xFF, 0xFF, 0xFF,
                 0xFF, 0xFF, 0xFF, 0xFF } {}

    Block(BlockFull UNUSED(full))
        : mHash{ 0xFE, 0xFE, 0xFE, 0xFE,
                 0xFE, 0xFE, 0xFE, 0xFE,
                 0xFE, 0xFE, 0xFE, 0xFE,
                 0xFE, 0xFE, 0xFE, 0xFE } {}

    bool
    is_empty(size_type i)
    const noexcept
    { return mHash[i % BLOCK_LEN] == EMPTY; }

    bool
    is_full(size_type i)
    const noexcept
    { return !is_empty(i); }

    bool
    is_head(size_type i)
    const noexcept
    { return !is_link(i); }

    bool
    is_link(size_type i)
    const noexcept
    { return mHash[i % BLOCK_LEN] & LINK; }

    bool
    is_end(size_type i)
    const noexcept
    { return 0 == mLeap[i % BLOCK_LEN]; }

    bool
    is_local(size_type i)
    const noexcept
    { return (mLeap[i % BLOCK_LEN] != FIND); }

    void
    flag_unsearchable(size_type i)
    noexcept
    { mHash[i % BLOCK_LEN] = NOFIND; }

    uint8_t
    get_hash(size_type i)
    const noexcept
    { return mHash[i % BLOCK_LEN]; }

    void
    set_hash(size_type i, uint8_t hash)
    noexcept
    { mHash[i % BLOCK_LEN] = hash; }

    uint8_t
    get_leap(size_type i)
    const noexcept
    { return mLeap[i % BLOCK_LEN]; }

    void
    set_leap(size_type i, uint8_t leap)
    noexcept
    { mLeap[i % BLOCK_LEN] = leap; }

    void
    set_find(size_type i)
    noexcept
    { mLeap[i % BLOCK_LEN] = FIND; }

    void
    set_end(size_type i)
    noexcept
    { mLeap[i % BLOCK_LEN] = 0; }

    void
    set_empty(size_type i)
    noexcept
    { mHash[i % BLOCK_LEN] = EMPTY; }

    Value&
    get_value(size_type i)
    noexcept
    { return *(mValue + (i % BLOCK_LEN)); }

    Value*
    get_value_ptr(size_type i)
    noexcept
    { return mValue + (i % BLOCK_LEN); }

    SearchMap
    find(uint8_t h)
    const noexcept
    {
#if defined __SSE2__
        // Documentation:
        // https://software.intel.com/sites/landingpage/IntrinsicsGuide/
        __m128i first = _mm_set1_epi8((char)h);
        __m128i second = _mm_loadu_si128((__m128i*)(mHash));
        return { _mm_movemask_epi8(_mm_cmpeq_epi8(first, second)) };
#else
        // Slower implementation available if we don't have SSE instructions.
        int result = 0;

        int i;
        for (i = 0; i < SLOTLEN; ++i)
        {
            if (slot->hashes[i] == searchhash)
            {
                result |= (1 << i);
            }
        }

        return { result };
#endif
    }

    SearchMap
    find_empty(size_type i)
    const noexcept
    {
        return { find_empty().value() & ~((1 << (i % BLOCK_LEN)) - 1) };
    }

    SearchMap
    find_empty()
    const noexcept
    {
        return { find(EMPTY) };
    }

    SearchMap
    find_full(size_type i)
    const noexcept
    {
        return { find_full().value() & ~((1 << (i % BLOCK_LEN)) - 1) };
    }

    SearchMap
    find_full()
    const noexcept
    {
        return { ~find(EMPTY).value() & ((1 << BLOCK_LEN) - 1) };
    }
};

static Block<uint8_t> NULL_BLOCK(BlockFull{});

template <int MaxLoadFactor,
          typename Key,
          typename T,
          typename Hash,
          typename Pred,
          typename Alloc
          >
class unordered_map: public Hash, public Pred, public Alloc
{
    using allocator_traits = std::allocator_traits<Alloc>;

public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const key_type, mapped_type>;
    using hasher = Hash;
    using key_equal = Pred;
    using allocator_type = Alloc;
    using block_type = Block<value_type>;
    using self_type =
        unordered_map<MaxLoadFactor,
                      key_type,
                      mapped_type,
                      hasher,
                      key_equal,
                      allocator_type>;

public:
    template <bool IsConstant>
    class Iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = typename self_type::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer =
            typename std::conditional<IsConstant,
                                      const value_type*,
                                      value_type*>::type;
        using reference =
            typename std::conditional<IsConstant,
                                      const value_type&,
                                      value_type&>::type;
        using block_type = typename self_type::block_type;

        Iterator() = delete;

        template <bool OtherIsConstant>
        Iterator(const Iterator<OtherIsConstant>& o)
            : mBlock(o.mBlock), mIndex(o.mIndex)
        {}

        Iterator(block_type* blockPointer, size_type index)
            : mBlock(blockPointer), mIndex(index)
        {}

        template <bool OtherIsConstant,
                  typename = typename std::enable_if<IsConstant && !OtherIsConstant>::type>
        Iterator&
        operator=(const Iterator<OtherIsConstant>& o)
        noexcept
        {
            mBlock = o.mBlock;
            mIndex = o.mIndex;
            return *this;
        }

        Iterator&
        operator++()
        noexcept
        {
            ++mIndex;
            leap_if_empty();
            return *this;
        }

        Iterator
        operator++(int)
        noexcept
        {
            return { mBlock, mIndex + 1 };
        }

        reference
        operator*()
        const
        {
            auto block = block_type::get(mBlock, mIndex);
            return block->get_value(mIndex);
        }

        pointer
        operator->()
        const
        {
            auto block = block_type::get(mBlock, mIndex);
            return block->get_value_ptr(mIndex);
        }

        template <bool OtherIsConstant>
        bool
        operator==(const Iterator<OtherIsConstant>& o)
        const noexcept
        {
            return mIndex == o.mIndex;
        }

        template <bool OtherIsConstant>
        bool
        operator!=(const Iterator<OtherIsConstant>& o)
        const noexcept
        {
            return mIndex != o.mIndex;
        }


    private:
        block_type* mBlock;
        size_type   mIndex;

        void
        leap_if_empty()
        {
            auto block = block_type::get(mBlock, mIndex);
            if (block->is_empty(mIndex))
            {
                SearchMap map = block->find_full(mIndex);
                while (!map.has())
                {
                    mIndex += 16;
                    block = block_type::get(mBlock, mIndex);
                    map = block->find_full();
                }
                mIndex = block_type::construct_index(mIndex, map.next());
            }
        }

        friend class unordered_map<MaxLoadFactor,
                                   key_type,
                                   mapped_type,
                                   hasher,
                                   key_equal,
                                   allocator_type>;
    };

private:
    static constexpr size_type MAX_SIZE = std::numeric_limits<size_type>::max() / 2;
    block_type* mBlock      = reinterpret_cast<block_type*>(&NULL_BLOCK);
    size_type   mSize       = 0;
    size_type   mLoad       = 0;
    size_type   mLen        = 0;
    size_type   mMask       = 0;

public:
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    unordered_map() = default;

    explicit
    unordered_map(size_type count,
                  const hasher& hash = hasher(),
                  const key_equal& equal = key_equal(),
                  const allocator_type& alloc = allocator_type())
        : hasher(hash), key_equal(equal), allocator_type(alloc)
    {
        reserve(count);
    }

    explicit
    unordered_map(const allocator_type& alloc)
        : allocator_type(alloc)
    {}

    template <class InputIterator>
    unordered_map(InputIterator start, InputIterator stop,
                  size_type count = 0,
                  const hasher& hash = hasher(),
                  const key_equal& equal = key_equal(),
                  const allocator_type& alloc = allocator_type())
        : hasher(hash), key_equal(equal), allocator_type(alloc)
    {
        reserve(count);
        insert(start, stop);
    }

    unordered_map(const unordered_map& o)
        : hasher(static_cast<const hasher&>(o)),
          key_equal(static_cast<const key_equal&>(o)),
          allocator_type(static_cast<const allocator_type&>(o))
    {
        if (o.mSize)
        {
            reserve(o.mSize);
            insert(o.cbegin(), o.cend());
        }
    }

    unordered_map(const unordered_map& o, const allocator_type& alloc)
        : hasher(static_cast<const hasher&>(o)),
          key_equal(static_cast<const key_equal&>(o)),
          allocator_type(alloc)
    {
        if (o.mSize)
        {
            reserve(o.mSize);
            insert(o.cbegin(), o.cend());
        }
    }

    unordered_map(unordered_map&& o)
        : hasher(std::move(static_cast<const hasher&>(o))),
          key_equal(std::move(static_cast<const key_equal&>(o))),
          allocator_type(std::move(static_cast<const allocator_type&>(o)))
    {
        if (o.mSize)
        {
            mBlock = std::move(o.mBlock);
            mSize = std::move(o.mSize);
            mLoad = std::move(o.mLoad);
            mLen = std::move(o.mLen);
            mMask = std::move(o.mMask);
        }
        else
        {
            o.reset();
        }
    }

    unordered_map(unordered_map&& o, const allocator_type& alloc)
        : hasher(std::move(static_cast<const hasher&>(o))),
          key_equal(std::move(static_cast<const key_equal&>(o))),
          allocator_type(alloc)
    {
        if (o.mSize)
        {
            mBlock = std::move(o.mBlock);
            mSize = std::move(o.mSize);
            mLoad = std::move(o.mLoad);
            mLen = std::move(o.mLen);
            mMask = std::move(o.mMask);
        }
        else
        {
            o.reset();
        }
    }

    unordered_map(std::initializer_list<value_type> il,
                  size_type count = 0,
                  const hasher& hash = hasher(),
                  const key_equal& equal = key_equal(),
                  const allocator_type& alloc = allocator_type())
        : hasher(hash), key_equal(equal), allocator_type(alloc)
    {
        reserve(count);
        insert(il.cbegin(), il.cend());
    }

    ~unordered_map()
    {
        destroy_values();
        deallocate_blocks(mBlock, mLen);
        mBlock = nullptr;
    }

    mapped_type&
    at(const key_type& k)
    {
        size_type index = find_index(k);
        if (index == mLen)
        {
            throw std::out_of_range("crj::unordered_map key not found");
        }
        auto block = get_block(index);
        return block->get_value(index).second;
    }

    const mapped_type&
    at(const key_type& k)
    const
    {
        size_type index = find_index(k);
        if (index == mLen)
        {
            throw std::out_of_range("crj::unordered_map key not found");
        }
        auto block = get_block(index);
        return block->get_value(index).second;
    }

    iterator
    begin()
    noexcept
    {
        return iterator{ mBlock, 0 };
    }

    const_iterator
    begin()
    const noexcept
    {
        return cbegin();
    }

    size_type
    bucket(const key_type& k)
    const
    {
        return find_index(k);
    }

    size_type
    bucket_count()
    const noexcept
    {
        return size();
    }

    size_type
    bucket_size(size_type index)
    const
    {
        auto block = get_block(index);
        return block->is_empty(index) ? 0 : 1;
    }

    const_iterator
    cbegin()
    const noexcept
    {
        return const_iterator{ mBlock, 0 };
    }

    const_iterator
    cend()
    const noexcept
    {
        return const_iterator{ mBlock, mLen };
    }

    void
    clear()
    noexcept
    {
        if (empty())
        {
            return;
        }

        destroy_values();
        clear_data();
        mSize = 0;
    }

    size_type
    count(const Key& k)
    const
    {
        return find_index(k) != mLen ? 1 : 0;
    }

    template <class... Args>
    std::pair<iterator, bool>
    emplace(Args&&... args)
    {
        return upsert<true, false, false>(std::forward<Args>(args)...);
    }

    template <class... Args>
    iterator
    emplace_hint(const_iterator UNUSED(position), Args&&... args)
    {
        return upsert<true, false, false>(std::forward<Args>(args)...);
    }

    bool
    empty()
    const noexcept
    {
        return 0 == mSize;
    }

    iterator
    end()
    noexcept
    {
        return iterator{ mBlock, mLen };
    }

    const_iterator
    end()
    const noexcept
    {
        return cend();
    }

    std::pair<iterator, iterator>
    equal_range(const key_type& k)
    {
        size_type index = find_index(k);
        if (index != mLen)
        {
            return { iterator{ mBlock, index }, iterator{ mBlock, index + 1}};
        }
        else
        {
            return { end(), end() };
        }
    }

    std::pair<const_iterator, const_iterator>
    equal_range(const key_type& k)
    const
    {
        size_type index = find_index(k);
        if (index != mLen)
        {
            return { const_iterator{ mBlock, index }, const_iterator{ mBlock, index + 1}};
        }
        else
        {
            return { cend(), cend() };
        }
    }

    iterator
    erase(const_iterator position)
    {
        erase((*position).first);
        return iterator{ position.mBlock, position.mIndex + 1 };
    }

    size_type
    erase(const key_type& k)
    {
        size_type hash = hash_key(k);
        size_type ihead = hash_to_index(hash);
        auto block = get_block(ihead);

        if (block->is_full(ihead))
        {
            if (LIKELY(block->is_head(ihead)))
            {
                uint8_t frag = hash_fragment(hash);

                if (frag == block->get_hash(ihead))
                {
                    if (compare_keys(block->get_value(ihead).first, k))
                    {
                        allocator_traits::destroy(*this, block->get_value_ptr(ihead));
                        if (LIKELY(block->is_end(ihead)))
                        {
                            block->set_empty(ihead);
                        }
                        else
                        {
                            unlink_head_of_list(ihead);
                        }
                        --mSize;
                        return 1;
                    }
                }

                if (block->is_end(ihead))
                {
                    return 0;
                }

                size_type index = ihead;
                frag = block_type::set_link_hash(frag);
                for (;;)
                {
                    bool notrust = false;
                    size_type iprev = index;
                    index = leap(ihead, index, notrust);
                    block = get_block(index);

                    if (frag == block->get_hash(index) || notrust)
                    {
                        if (compare_keys(block->get_value(index).first, k))
                        {
                            unlink(ihead, iprev, index);
                            block->set_empty(index);
                            allocator_traits::destroy(*this, block->get_value_ptr(index));
                            --mSize;
                            return 1;
                        }
                    }

                    if (block->is_end(index))
                    {
                        return 0;
                    }
                }
            }
        }
        
        return 0;
    }

    iterator
    erase(const_iterator first, const_iterator last)
    {
        iterator start = iterator{ first.mBlock, first.mIndex };
        while (start != last)
        {
            erase((*start).first);
            ++start;
        }
        return start;
    }

    iterator
    find(const key_type& k)
    {
        const size_type index = find_index(k);
        return iterator{ mBlock, index };
    }

    const_iterator
    find(const key_type& k)
    const
    {
        const size_type index = find_index(k);
        return const_iterator{ mBlock, index };
    }

    allocator_type
    get_allocator()
    const noexcept
    {
        return allocator_type{};
    }

    hasher
    hash_function()
    const
    {
        return hasher{};
    }

    std::pair<iterator, bool>
    insert(const value_type& val)
    {
        return upsert<false, false, false>(val);
    }

    std::pair<iterator, bool>
    insert(value_type&& val)
    {
        return upsert<false, false, false>(std::move(val));
    }

    template <typename P>
    std::pair<iterator, bool>
    insert(P&& val)
    {
        return upsert<false, false, false>(std::move(val));
    }

    iterator
    insert(const_iterator UNUSED(hint), const value_type& val)
    {
        return upsert<false, false, false>(val).first;
    }

    template <typename P>
    iterator
    insert(const_iterator UNUSED(hint), P&& val)
    {
        return upsert<false, false, false>(std::move(val)).first;
    }

    template <typename InputIterator>
    void
    insert(InputIterator first, InputIterator last)
    {
        while (first != last)
        {
            upsert<false, false, false>(value_type(*first));
            ++first;
        }
    }

    void
    insert(std::initializer_list<value_type> il)
    {
        insert(il.begin(), il.end());
    }

    key_equal
    key_eq()
    const
    {
        return key_equal{};
    }

    float
    load_factor()
    const
    {
        return static_cast<float>(mSize) / static_cast<float>(mLen);
    }

    size_type
    max_bucket_count()
    const noexcept
    {
        return max_size();
    }

    float
    max_load_factor()
    const
    {
        return static_cast<float>(MaxLoadFactor) / 100.0F;
    }

    size_type
    max_size()
    const noexcept
    {
        return MAX_SIZE;
    }

    unordered_map&
    operator=(const unordered_map& o)
    {
        if (this == &o)
        {
            return *this;
        }

        if (mLen != o.mLen)
        {
            reset();
            reserve(o.mSize);
        }
        else
        {
            clear();
        }

        hasher::operator=(static_cast<const hasher&>(o));
        key_equal::operator=(static_cast<const key_equal&>(o));
        allocator_type::operator=(static_cast<const allocator_type&>(o));

        insert(o.cbegin(), o.cend());

        return *this;
    }

    unordered_map&
    operator=(unordered_map&& o)
    noexcept
    {
        if (this == &o)
        {
            return *this;
        }

        if (o.mSize)
        {
            deallocate_blocks(mBlock, mLen);
            mBlock = std::move(o.mBlock);
            mSize = std::move(o.mSize);
            mLoad = std::move(o.mLoad);
            mLen = std::move(o.mLen);
            mMask = std::move(o.mMask);
        }
        else
        {
            clear();
            o.reset();
        }

        hasher::operator=(static_cast<const hasher&>(o));
        key_equal::operator=(static_cast<const key_equal&>(o));
        allocator_type::operator=(static_cast<const allocator_type&>(o));

        return *this;
    }

    unordered_map&
    operator=(std::initializer_list<value_type> il)
    {
        clear();
        insert(il.begin(), il.end());
        return *this;
    }

    template <typename OtherMap>
    bool
    operator==(const OtherMap& o)
    const
    {
        if (size() != o.size())
        {
            return false;
        }

        iterator start{ mBlock, 0 };
        const_iterator stop = cend();

        while (start != stop)
        {
            if (0 == o.count((*start).first))
            {
                return false;
            }
            ++start;
        }
        
        return true;
    }

    template <typename OtherMap>
    bool
    operator!=(const OtherMap& o)
    const
    {
        return !operator==(o);
    }

    mapped_type&
    operator[](const key_type& k)
    {
        return upsert<true, false, false>(k, value_type()).first->second;
    }

    mapped_type&
    operator[](key_type&& k)
    {
        return upsert<true, false, false>(std::move(k), value_type()).first->second;
    }

    void
    rehash(size_type n)
    {
        if (mLen < n)
        {
            resize_to(n);
        }
        else if (mSize < n)
        {
            resize_to(n);
        }
    }

    void
    reset()
    noexcept
    {
        destroy_values();
        deallocate_blocks(mBlock, mLen);
        mBlock = reinterpret_cast<block_type*>(&NULL_BLOCK);
        mSize = 0;
        mLoad = 0;
        mLen = 0;
        mMask = 0;
    }

    void
    reserve(size_type count)
    {
        if (mLoad < count)
        {
            size_type lenFor = force_load(count);
            resize_to(lenFor);
        }
    }

    size_type
    size()
    const noexcept
    {
        return mSize;
    }

    void
    swap(unordered_map& o)
    {
        if (this == &o)
        {
            return;
        }

        std::swap(*this, o);
    }

private:
    template <typename FindKey>
    size_type
    find_index(const FindKey& k)
    const
    {
        size_type hash = hash_key(k);
        size_type ihead = hash_to_index(hash);
        auto block = get_block(ihead);

        if (block->is_empty(ihead))
        {
            return mLen;
        }

        if (UNLIKELY(block->is_link(ihead)))
        {
            return mLen;
        }

        uint8_t frag = hash_fragment(hash);
        size_type index = ihead;

        if (frag == block->get_hash(index))
        {
            if (compare_keys(block->get_value(index).first, k))
            {
                return index;
            }
        }

        if (block->is_end(index))
        {
            return mLen;
        }

        frag = block_type::set_link_hash(frag);
        for (;;)
        {
            bool notrust = false;
            index = leap(ihead, index, notrust);
            block = get_block(index);

            if (frag == block->get_hash(index) || notrust)
            {
                if (compare_keys(block->get_value(index).first, k))
                {
                    return index;
                }
            }

            if (block->is_end(index))
            {
                return mLen;
            }
        }

        return mLen;
    }

    NOINLINE
    void
    unlink_head_of_list(size_type ihead)
    {
        bool noTrustFirst = false;
        size_type iprev = ihead;
        size_type itail = leap(ihead, iprev, noTrustFirst);
        auto blocktail = get_block(itail);

        bool noTrustFinal = false;
        while (!blocktail->is_end(itail))
        {
            iprev = itail;
            itail = leap(ihead, iprev, noTrustFinal);
            blocktail = get_block(itail);
        }

        auto blockhead = get_block(ihead);
        ::new (blockhead->get_value_ptr(ihead))
            value_type(std::move(blocktail->get_value(itail)));

        block_type* blockprev;
        if (LIKELY(iprev == ihead))
        {
            blockprev = blockhead;
        }
        else
        {
            blockprev = get_block(iprev);
        }

        uint8_t frag;
        if (UNLIKELY(noTrustFinal))
        {
            frag = hash_fragment(hash_key(blockhead->get_value(ihead).first));
        }
        else
        {
            frag = block_type::clear_link_hash(blocktail->get_hash(itail));
        }

        blockprev->set_end(iprev);
        blocktail->set_empty(itail);

        if (UNLIKELY(noTrustFirst && iprev != ihead))
        {
            cascade(ihead, ihead, frag);
        }

        blockhead->set_hash(ihead, frag);
    }

    void
    unlink_link_at(size_type index)
    {
        auto block = get_block(index);
        size_type ihead = key_to_index(block->get_value(index).first);
        size_type iprev = ihead;
        bool notrust;
        size_type inext = leap(ihead, iprev, notrust);

        while (inext != index)
        {
            iprev = inext;
            inext = leap(ihead, iprev, notrust);
        }

        unlink(ihead, iprev, index);
    }

    template <bool DoUpsert,
              bool IsUnique,
              bool IsListInsert,
              typename UpsertKey,
              typename... Args>
    std::pair<iterator, bool>
    upsert(UpsertKey&& k, Args&&... args)
    {
        size_type hash = hash_key(k);
        uint8_t frag = hash_fragment(hash);

        for (;;)
        {
            size_type ihead = hash_to_index(hash);
            auto block = get_block(ihead);
            uint8_t newleap = 0;
            size_type index = ihead;

            if (IsListInsert || block->is_full(ihead))
            {
                if (!IsListInsert)
                {
                    if (UNLIKELY(needToGrow()))
                    {
                        grow();
                        continue;
                    }
                }

                if (!IsListInsert && UNLIKELY(block->is_link(ihead)))
                {
                    unlink_link_at(ihead);
                    block->flag_unsearchable(ihead);
                    upsert<true, true, true>(std::move(block->get_value(ihead)));
                }
                else
                {
                    do
                    {
                        if (!IsUnique && (frag == block->get_hash(index)))
                        {
                            if (compare_keys(block->get_value(index).first, k))
                            {
                                if (DoUpsert)
                                {
                                    allocator_traits::destroy(*this, block->get_value_ptr(index));
                                    allocator_traits::construct(*this, block->get_value_ptr(index), std::forward<UpsertKey>(k), std::forward<Args>(args)...);
                                }

                                return std::make_pair<iterator, bool>({mBlock, index}, false);
                            }
                        }

                        if (block->is_end(index))
                        {
                            break;
                        }

                        frag = block_type::set_link_hash(frag);
                        for (;;)
                        {
                            bool notrust = false;
                            index = leap(ihead, index, notrust);
                            block = get_block(index);

                            if (!IsUnique
                                && (frag == block->get_hash(index)
                                    || notrust))
                            {
                                if (compare_keys(block->get_value(index).first, k))
                                {
                                    if (DoUpsert)
                                    {
                                        allocator_traits::destroy(*this, block->get_value_ptr(index));
                                        ::new (block->get_value_ptr(index)) value_type(std::forward<UpsertKey>(k), std::forward<Args>(args)...);
                                    }

                                    return std::make_pair<iterator, bool>({mBlock, index}, false);
                                }
                            }

                            if (block->is_end(index))
                            {
                                break;
                            }
                        }
                    }
                    while (false);

                    index = link_empty(ihead, index, frag);
                    block = get_block(index);
                }
            }

            block->set_hash(index, frag);
            block->set_leap(index, newleap);
            ::new (block->get_value_ptr(index)) value_type(std::forward<UpsertKey>(k), std::forward<Args>(args)...);
            ++mSize;
            return std::make_pair<iterator, bool>({mBlock, index}, true);
        }
    }

    size_type
    find_empty(size_type itail)
    const noexcept
    {
        auto block = get_block(itail);
        int isub;

        SearchMap map = block->find_empty(itail);
        if (LIKELY(map.has()))
        {
            isub = map.next();
            return combine_index(itail, isub);
        }

        size_type index = (itail + BLOCK_LEN) & mMask;
        for (;;)
        {
            block = get_block(index);
            map = block->find_empty();
            if (map.has())
            {
                isub = map.next();
                return combine_index(index, isub);
            }
            index = (index + BLOCK_LEN) & mMask;
        }
    }

    size_type
    link_empty(size_type ihead, size_type itail, uint8_t& shash)
    noexcept
    {
        size_type iempty = find_empty(itail);
        size_type emptyPos = ((iempty + mLen) - ihead) & mMask;
        size_type nextPos = ((itail + mLen) - ihead) & mMask;

        if (LIKELY(emptyPos > nextPos))
        {
            link(itail, iempty, shash);
        }
        else
        {
            // Location of empty between head and tail.
            size_type iprev = ihead;
            bool scrap;
            size_type inext = leap(ihead, ihead, scrap);

            for (;;) 
            {
                nextPos = ((inext + mLen) - ihead) & mMask;
                if (emptyPos < nextPos)
                {
                    // Found the indices the empty lies between.
                    break;
                }
                iprev = inext;
                inext = leap(ihead, inext, scrap);
            }

            auto empty = get_block(iempty);
            auto next = get_block(inext);

            // Link previous to empty slot.
            link(iprev, iempty, shash);
            // Set the empty slot.
            empty->set_hash(iempty, shash);
            // Generate hash of next and link empty to next.
            size_type hash = hash_key(next->get_value(inext).first);
            uint8_t subhashnext = hash_fragment(hash);
            link(iempty, inext, subhashnext);
            // Check if we need to cascade hashes.
            if (!next->is_local(inext))
            {
                bool scrap;
                size_type inextnext = leap(ihead, inext, scrap);
                cascade(ihead, inextnext, subhashnext);
            }
            // Set possibly new subhash.
            next->set_hash(inext, subhashnext);
        }
        
        return iempty;
    }

    void
    link(size_type iprev, size_type inext, uint8_t& shash)
    {
        // We convert dist to uint8_t leap value at end.
        size_type dist = index_dist(iprev, inext);
        auto block = get_block(iprev);

        if (LIKELY(block_type::can_leap(dist)))
        {
            block->set_leap(iprev, dist);
        }
        else
        {
            shash = block->get_hash(iprev);
            block->set_find(iprev);
        }
    }

    void
    unlink_complex(size_type ihead, size_type iprev, size_type iunlink)
    {
        auto un = get_block(iunlink);
        auto prev = get_block(iprev);

        // If the new leap stays local then we have an efficient solution.
        if (prev->is_local(iprev) && un->is_local(iunlink))
        {
            size_type dist = (size_type)(prev->get_leap(iprev))
                           + (size_type)(un->get_leap(iunlink));
            if (LIKELY(block_type::can_leap(dist)))
            {
                prev->set_leap(iprev, (uint8_t)dist);
                return;
            }
        }

        // Calculate the distance from prev to next.
        bool scrap;
        size_type inext = leap(ihead, iprev, scrap);

        // Propagate the subhash.
        uint8_t subhashprev = prev->get_hash(iprev);
        subhashprev = block_type::set_link_hash(subhashprev);
        cascade(ihead, inext, subhashprev);

        // Good, we can just replace the previous leap.
        prev->set_find(iprev);
    }

    INLINE
    void
    unlink(size_type ihead, size_type iprev, size_type iunlink)
    {
        // Remove entry from linked list.
        auto un = get_block(iunlink);
        auto prev = get_block(iprev);

        if (un->is_end(iunlink))
        {
            prev->set_end(iprev);
        }
        else
        {
            unlink_complex(ihead, iprev, iunlink);
        }
    }

    NOINLINE
    void
    cascade(size_type ihead, size_type inext, uint8_t newsubhash)
    {
        auto block = get_block(inext);

        for (;;)
        {
            if (block->is_end(inext))
            {
                // If the next entry is the end, we're done.
                break;
            }

            if (block->is_local(inext))
            {
                // If the next entry isn't a search, we're done.
                break;
            }
            
            // Perform the search BEFORE we change the search hash.
            size_type ifrom = (inext + block->get_leap(inext)) & mLen;
            size_type inextnext = extended_leap(ihead, ifrom,
                                                block->get_hash(inext));
            // We can change the next's subhash.
            block->set_hash(inext, newsubhash);

            // Advance the slot pointer.
            inext = inextnext;
            block = get_block(inext);
        }

        // We can change the next's subhash.
        block->set_hash(inext, newsubhash);
    }


    void
    decrease_load(size_type entryLen)
    {
        size_type prevLoad = mLoad;
        mLoad = size_type(((double)MaxLoadFactor / 100.0) * (double)entryLen);
        if (mLoad < mSize)
        {
            mLoad = prevLoad / 2;
        }
        if (mLoad < (entryLen / 2))
        {
            mLoad = entryLen / 2;
        }
        if (mLoad > entryLen)
        {
            mLoad = entryLen;
        }
    }

    void
    increase_load(size_type entryLen)
    {
        size_type prevLoad = mLoad;
        mLoad = size_type(((double)MaxLoadFactor / 100.0) * (double)entryLen);
        if (mLoad < prevLoad)
        {
            mLoad = prevLoad * 2;
        }
        if (mLoad < (entryLen / 2))
        {
            mLoad = entryLen / 2;
        }
        if (mLoad > entryLen)
        {
            mLoad = entryLen;
        }
    }

    size_type
    force_load(size_type minLoad)
    const noexcept
    {
        return size_type((100.0 / (double)MaxLoadFactor) * (double)minLoad);
    }

    void
    update_load(size_type len)
    {
        mLoad = size_type(((double)MaxLoadFactor / 100.0) * (double)len);
    }

    size_type
    hash_key(const value_type& kv)
    {
        return hasher::operator()(kv.first);
    }

    size_type
    hash_key(const value_type& kv)
    const
    {
        return hasher::operator()(kv.first);
    }

    template <typename HashKey>
    size_type
    hash_key(const HashKey& k)
    {
        return hasher::operator()(k);
        //return static_cast<hasher&>(*this)(k);
    }

    template <typename HashKey>
    size_type
    hash_key(const HashKey& k)
    const
    {
        return hasher::operator()(k);
        //return static_cast<const hasher&>(*this)(k);
    }

    template <typename LeftKey>
    bool
    compare_keys(const LeftKey& l, const value_type& r)
    const noexcept
    {
        return key_equal::operator()(l, r.first);
    }

    template <typename LeftKey, typename RightKey>
    bool
    compare_keys(const LeftKey& l, const RightKey& r)
    const noexcept
    {
        return key_equal::operator()(l, r);
        //return static_cast<key_equal&>(*this)(l, r);
    }

    size_type
    hash_to_index(size_type hash)
    const noexcept
    {
        return hash & mMask;
    }

    template <typename IndexKey>
    size_type
    key_to_index(IndexKey&& k)
    const noexcept
    {
        return hash_to_index(hash_key(k));
    }

    size_type
    index_dist(size_type istart, size_type iend)
    {
        return ((iend + mLen) - istart) & mMask;
    }

    size_type
    block_dist(size_type istart, size_type iend)
    {
        istart = istart / BLOCK_LEN;
        iend = iend / BLOCK_LEN;
        return ((iend + (mLen / BLOCK_LEN)) - istart);
    }

    block_type*
    get_block(size_type index)
    const noexcept
    {
        return block_type::get(mBlock, index);
    }

    size_type
    combine_index(size_type index, int isub)
    const noexcept
    {
        return (index & ~(BLOCK_LEN - 1)) + isub;
    }

    uint8_t
    hash_fragment(size_type hash)
    const noexcept
    {
        return hash >> ((sizeof(size_type)*8) - 6);
    }

    INLINE
    size_type
    leap(size_type ihead, size_type index, bool& notrust)
    const noexcept
    {
        auto block = get_block(index);
        index = (index + block->get_leap(index)) & mMask;
        if (LIKELY(block->is_local(index)))
        {
            notrust = false;
            return index;
        }
        else
        {
            notrust = true;
            return extended_leap(ihead, index, block->get_hash(index));
        }
    }

    /** @brief Cannot compare subhash with "leap'd to" entry. */
    NOINLINE
    size_type
    extended_leap(size_type ihead, size_type ifrom, uint8_t findhash)
    const noexcept
    {
        // Linear search through hashes.
        // Use same subhash as leap entry for search efficiency.

        // Iterate through every slot in the table starting at the leap point.
        for (;;)
        {
            auto block = get_block(ifrom);
            SearchMap map = block->find(findhash);
            // For each entry in the map.
            while (map.has())
            {
                // Discover the index that we're jumping to and test it
                // to see if it is part of the same linked list.
                int isub = map.next();
                size_type iheadtest = key_to_index(block->get_value(isub).first);
                if (ihead == iheadtest)
                {
                    return combine_index(ifrom, isub);
                }
                map.clear(isub);
            }

            ifrom = (ifrom + BLOCK_LEN) & mMask;
        }
    }

    bool
    needToGrow()
    const noexcept
    {
        return mSize >= mLoad;
    }

    void
    grow()
    {
        size_type newLen = mLen * 2;

        if (UNLIKELY(newLen < mLen))
        {
            throw std::overflow_error("crj::unordered_map size overflow");
        }

        resize_to(newLen);
    }

    size_type
    to_power_2(size_type n)
    {
        if (!n)
        {
            return 1;
        }

        if (!(n & (n - 1)))
        {
            return n;
        }

        if (n > (std::numeric_limits<size_type>::max() / 4))
        {
            return size_type(1) << ((sizeof(size_type) * 8) - 2);
        }

        size_type p = 1;

        while (p < n)
        {
            p <<= 1;
        }

        return p;
    }

    NOINLINE
    void
    resize_to(size_type minLen)
    {
        size_type lenPwr2 = to_power_2(minLen);

        if (lenPwr2 < BLOCK_LEN)
        {
            lenPwr2 = BLOCK_LEN;
        }

        if (lenPwr2 == mLen)
        {
            return;
        }

        block_type* oldBlock = mBlock;
        size_type oldLen = mLen;

        mBlock = allocate_blocks(lenPwr2);
        mLen = lenPwr2;
        mMask = lenPwr2 - 1;
        update_load(mLen);

        if (reinterpret_cast<block_type*>(&NULL_BLOCK) == oldBlock)
        {
            return;
        }

        if (mSize)
        {
            iterator start{ oldBlock, 0 };
            const_iterator stop{ oldBlock, oldLen };

            while (start != stop)
            {
                upsert<false, true, false>(*start);
                ++start;
            }
        }

        deallocate_blocks(oldBlock, oldLen);
    }

    size_type
    memory_size(size_type len)
    const noexcept
    {
        return sizeof(block_type) * (len / BLOCK_LEN);
    }

    block_type*
    allocate_blocks(size_type len)
    {
        size_type memory = memory_size(len);

        unsigned char *p = allocator_traits::allocate(*this, memory);
        block_type::set_empty_all(p, memory);
        block_type* b = reinterpret_cast<block_type*>(p);
        return b;
    }

    void
    deallocate_blocks(block_type* b, size_type len)
    {
        if (reinterpret_cast<block_type*>(&NULL_BLOCK) != b)
        {
            size_type memory = memory_size(len);
            allocator_traits::deallocate(*this, reinterpret_cast<typename allocator_traits::pointer>(b), memory);
        }
    }

    void
    destroy_values()
    noexcept
    {
        iterator start = begin();
        const_iterator stop = cend();

        while (start != stop)
        {
            allocator_traits::destroy(*this, &(*start));
            ++start;
        }
    }

    void
    clear_data()
    noexcept
    {
        size_type memory = memory_size(mLen);
        block_type::set_empty_all(mBlock, memory);
    }
};


} /* namespace detail */

template <typename Key,
          typename T,
          typename Hash = FibonacciHash<Key>,
          typename Pred = std::equal_to<Key>,
          typename Alloc = std::allocator<std::pair<Key, T> >
          >
using unordered_map =
    detail::unordered_map<99,
                          Key,
                          T,
                          Hash,
                          Pred,
                          typename std::allocator_traits<Alloc>::template
                          rebind_alloc<unsigned char>
                          >;

} /* namespace crj */


#endif /* CRJ_HASH_MAP_H */

