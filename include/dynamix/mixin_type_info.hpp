// DynaMix
// Copyright (c) 2013-2019 Borislav Stanimirov, Zahary Karadjov
//
// Distributed under the MIT Software License
// See accompanying file LICENSE.txt or copy at
// https://opensource.org/licenses/MIT
//
#pragma once


/**
 * \file
 * Defines the internal classes that describe the information about a mixin -
 * features and message data, type name, size, alignment, etc.
 */

#include "config.hpp"
#include "mixin_id.hpp"
#include "message.hpp"
#include "metrics.hpp"

#include <type_traits>
#include <utility>
#include <vector>

namespace dynamix
{

class mixin_allocator;

static const mixin_id INVALID_MIXIN_ID = ~mixin_id(0);

typedef void(*mixin_constructor_proc)(void* memory);
typedef void(*mixin_copy_proc)(void* memory, const void* source);
typedef void(*mixin_move_proc)(void* memory, void* source);
typedef void(*mixin_destructor_proc)(void* memory);

/**
* Mixin type info. Contains a the mixin type information, features and traits.
*/
class DYNAMIX_API mixin_type_info
{
public:
    mixin_type_info() = default;

    /// The mixin's id
    mixin_id id = INVALID_MIXIN_ID;

    /// Shows whether this is as initialized mixin type
    bool is_valid() const { return id != INVALID_MIXIN_ID; }

    /// The mixin name: The class name or, in case `mixin_name` feature is provided,
    /// the manual name set from there
    const char* name = nullptr;

    /// Size of the mixin type
    size_t size = 0;

    /// Alignment of the mixin type
    size_t alignment = 0;

    /// Allocator associated with this mixin type.
    /// If no special one was provided this will be equal to the allocator of the domain.
    mixin_allocator* allocator = nullptr;

    /// Procedure which calls the default constructor of a mixin.
    /// Might be left null if the mixin is not default-constructible.
    mixin_constructor_proc constructor = 0;

    /// Procedure which calls the destructor of a mixin. Never null.
    mixin_destructor_proc destructor = 0;

    /// Procedutre which calls the copy-constructor of a mixin.
    /// Might be left null for mixins which aren't copy-constructible
    mixin_copy_proc copy_constructor = 0;

    /// Procedure which calls the copy assignment of a mixin
    /// Might be left null for mixins which aren't copy-assignable
    mixin_copy_proc copy_assignment = 0;

    /// Procedure which calls the move-constrcutor of a mixin
    /// Might be left null for mixin which aren't move-constructible
    mixin_move_proc move_constructor = 0;

    /// Procedure which calls the move assignment of a mixin
    /// Might be left null for mixin which aren't move-constructible
    mixin_move_proc move_assignment = 0;

    /// All the message infos for the messages this mixin supports
    std::vector<internal::message_for_mixin> message_infos;

#if DYNAMIX_USE_TYPEID && defined(__GNUC__)
    // boolean which shows whether the name in the mixin type info was obtained
    // by cxa demangle and should be freed
    bool owns_name = false;
#endif

#if DYNAMIX_ADDITIONAL_METRICS
    /// Number of "living" mixins of this type.
    mutable metric num_mixins = {0};
#endif

    // non-copyable
    mixin_type_info(const mixin_type_info&) = delete;
    mixin_type_info& operator=(const mixin_type_info&) = delete;

    // non-movable
    mixin_type_info(mixin_type_info&&) = delete;
    mixin_type_info& operator=(mixin_type_info&&) = delete;
};

namespace internal
{

// getters of type info based on typeid
#if DYNAMIX_USE_TYPEID
    extern DYNAMIX_API const char* get_mixin_name_from_typeid(const char* typeid_name);
#   if defined(__GNUC__)
    extern DYNAMIX_API void free_mixin_name_from_typeid(const char* typeid_name);
#   endif
#endif

// this metafunction binds the type info of a mixin to its type
template <typename Mixin>
struct mixin_type_info_instance
{
    // have this static function instead of a simple member to guarantee
    // that mixin_type_info's constructor is called the first time
    static mixin_type_info& info()
    {
        static mixin_type_info d;
        return d;
    }

    // this static member registers the mixin in the domain
    // we need to reference it somewhere so as to call its constructor
    static mixin_type_info_instance registrator;

    // the constructor is defined in mixin.h because it references the domain object
    mixin_type_info_instance();

    ~mixin_type_info_instance();

    // non-copyable
    mixin_type_info_instance(const mixin_type_info_instance&) = delete;
    mixin_type_info_instance& operator=(const mixin_type_info_instance&) = delete;

    // to prevent warnings and optimizations that will say that we're not using
    // mixin_type_info_instance by simply referencing it
    int unused;
};
template <typename Mixin>
mixin_type_info_instance<Mixin> mixin_type_info_instance<Mixin>::registrator;

// this procedure is used for the mixin construction
template <typename Mixin>
void call_mixin_constructor(void* memory)
{
    new (memory) Mixin;
}

template <typename Mixin>
void call_mixin_destructor(void* memory)
{
    reinterpret_cast<Mixin*>(memory)->~Mixin();
}

template <typename Mixin>
void call_mixin_copy_constructor(void* memory, const void* source)
{
    new (memory) Mixin(*reinterpret_cast<const Mixin*>(source));
}

template <typename Mixin>
void call_mixin_copy_assignment(void* target, const void* source)
{
    *reinterpret_cast<Mixin*>(target) = *reinterpret_cast<const Mixin*>(source);
}

template <typename Mixin>
typename std::enable_if<std::is_copy_constructible<Mixin>::value,
    mixin_copy_proc>::type get_mixin_copy_constructor()
{
    return call_mixin_copy_constructor<Mixin>;
}

template <typename Mixin>
typename std::enable_if<!std::is_copy_constructible<Mixin>::value,
    mixin_copy_proc>::type get_mixin_copy_constructor()
{
    return nullptr;
}

template <typename Mixin>
typename std::enable_if<std::is_copy_assignable<Mixin>::value,
    mixin_copy_proc>::type get_mixin_copy_assignment()
{
    return call_mixin_copy_assignment<Mixin>;
}

template <typename Mixin>
typename std::enable_if<!std::is_copy_assignable<Mixin>::value,
    mixin_copy_proc>::type get_mixin_copy_assignment()
{
    return nullptr;
}

template <typename Mixin>
void call_mixin_move_constructor(void* memory, void* source)
{
    new (memory) Mixin(std::move(*reinterpret_cast<Mixin*>(source)));
}

template <typename Mixin>
typename std::enable_if<std::is_move_constructible<Mixin>::value,
    mixin_move_proc>::type get_mixin_move_constructor()
{
    return call_mixin_move_constructor<Mixin>;
}

template <typename Mixin>
typename std::enable_if<!std::is_move_constructible<Mixin>::value,
    mixin_move_proc>::type get_mixin_move_constructor()
{
    return nullptr;
}

template <typename Mixin>
void call_mixin_move_assignment(void* target, void* source)
{
    *reinterpret_cast<Mixin*>(target) = std::move(*reinterpret_cast<Mixin*>(source));
}

template <typename Mixin>
typename std::enable_if<std::is_move_assignable<Mixin>::value,
    mixin_move_proc>::type get_mixin_move_assignment()
{
    return call_mixin_move_assignment<Mixin>;
}

template <typename Mixin>
typename std::enable_if<!std::is_move_assignable<Mixin>::value,
    mixin_move_proc>::type get_mixin_move_assignment()
{
    return nullptr;
}

} // namespace internal
} // namespace dynamix
