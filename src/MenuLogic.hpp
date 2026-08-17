#pragma once

#include <vector>

/**
 * @brief Menu navigation that wraps while skipping disabled items.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 */
namespace MenuLogic
{

/**
 * @struct ItemList
 * @brief For nonempty menus, selected must be in bounds before navigation; helpers do not repair
 * invalid indices.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 */
struct ItemList
{
    /// False entries cannot be selected by navigation.
    std::vector<bool> enabled;

    int selected = 0;
};

/**
 * @fn int MenuLogic::FirstEnabledIndex(const ItemList& list)
 * @brief 0 when no item is enabled.
 * @author Alex (<https://github.com/lextpf>)
 */
inline int FirstEnabledIndex(const ItemList& list)
{
    for (int i = 0; i < static_cast<int>(list.enabled.size()); ++i)
    {
        if (list.enabled[i])
        {
            return i;
        }
    }
    return 0;
}

namespace detail
{
inline bool AnyEnabledOtherThan(const ItemList& list, int idx)
{
    for (int i = 0; i < static_cast<int>(list.enabled.size()); ++i)
    {
        if (i != idx && list.enabled[i])
        {
            return true;
        }
    }
    return false;
}
}  // Namespace detail

/**
 * @fn void MenuLogic::NavigateDown(ItemList& list)
 * @brief Wrap and skip disabled items; retain selection when it is the only enabled item.
 * @author Alex (<https://github.com/lextpf>)
 */
inline void NavigateDown(ItemList& list)
{
    const int n = static_cast<int>(list.enabled.size());
    if (n == 0 || !detail::AnyEnabledOtherThan(list, list.selected))
    {
        return;
    }
    int next = list.selected;
    for (int step = 0; step < n; ++step)
    {
        next = (next + 1) % n;
        if (list.enabled[next])
        {
            list.selected = next;
            return;
        }
    }
}

/**
 * @fn void MenuLogic::NavigateUp(ItemList& list)
 * @brief Wrap and skip disabled items; retain selection when it is the only enabled item.
 * @author Alex (<https://github.com/lextpf>)
 */
inline void NavigateUp(ItemList& list)
{
    const int n = static_cast<int>(list.enabled.size());
    if (n == 0 || !detail::AnyEnabledOtherThan(list, list.selected))
    {
        return;
    }
    int next = list.selected;
    for (int step = 0; step < n; ++step)
    {
        next = (next - 1 + n) % n;
        if (list.enabled[next])
        {
            list.selected = next;
            return;
        }
    }
}

/**
 * @enum ConfirmChoice
 * @brief The two answers to a confirmation prompt (e.g., "overwrite save?").
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 */
enum class ConfirmChoice : uint8_t
{
    /// Default choice so an accidental confirmation does not act.
    Cancel,
    Confirm
};

/**
 * @struct ConfirmPrompt
 * @brief State of one two-option confirmation prompt.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 */
struct ConfirmPrompt
{
    /// Left and right saturate without wrapping.
    ConfirmChoice selected = ConfirmChoice::Cancel;
};

inline void ConfirmRight(ConfirmPrompt& p)
{
    p.selected = ConfirmChoice::Confirm;
}

inline void ConfirmLeft(ConfirmPrompt& p)
{
    p.selected = ConfirmChoice::Cancel;
}

}  // Namespace MenuLogic
