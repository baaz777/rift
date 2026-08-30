#pragma once

#include "DialogueHandle.hpp"

#include <string>

/**
 * @struct Dialogue
 * @brief NPC dialogue identity and tree handle.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Dialogue
 */
struct Dialogue
{
    std::string type;
    std::string name;
    std::string text;
    DialogueHandle tree;  ///< Handle into the DialogueStore; 0 = no tree.
};
