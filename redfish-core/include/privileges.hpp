/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#pragma once

#include "logging.hpp"
#include "sessions.hpp"

#include <boost/beast/http/verb.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/vector.hpp>
#include <boost/move/algo/move.hpp>

#include <array>
#include <bitset>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// IWYU pragma: no_include <stddef.h>

namespace redfish
{

enum class PrivilegeType
{
    BASE,
    OEM
};

/** @brief A fixed array of compile time privileges  */
constexpr std::array<const char*, 5> basePrivileges{
    "Login", "ConfigureManager", "ConfigureComponents", "ConfigureSelf",
    "ConfigureUsers"};

constexpr const size_t basePrivilegeCount = basePrivileges.size();

/** @brief Max number of privileges per type  */
constexpr const size_t maxPrivilegeCount = 32;

/**
 * @brief A vector of all privilege names and their indexes
 * The privilege "OpenBMCHostConsole" is added to users who are members of the
 * "hostconsole" user group. This privilege is required to access the host
 * console.
 */
static const std::array<std::string, maxPrivilegeCount> privilegeNames{
    "Login",         "ConfigureManager", "ConfigureComponents",
    "ConfigureSelf", "ConfigureUsers",   "OpenBMCHostConsole"};

/**
 * @brief Redfish privileges
 *
 *        This implements a set of Redfish privileges.  These directly represent
 *        user privileges and help represent entity privileges.
 *
 *        Each incoming Connection requires a comparison between privileges held
 *        by the user issuing a request and the target entity's privileges.
 *
 *        To ensure best runtime performance of this comparison, privileges
 *        are represented as bitsets. Each bit in the bitset corresponds to a
 *        unique privilege name.
 *
 *        A bit is set if the privilege is required (entity domain) or granted
 *        (user domain) and false otherwise.
 *
 */
class Privileges
{
  public:
    /**
     * @brief Constructs object without any privileges active
     *
     */
    Privileges() = default;

    /**
     * @brief Constructs object with given privileges active
     *
     * @param[in] privilegeList  List of privileges to be activated
     *
     */
    Privileges(std::initializer_list<const char*> privilegeList)
    {
        for (const char* privilege : privilegeList)
        {
            if (!setSinglePrivilege(privilege))
            {
                BMCWEB_LOG_CRITICAL("Unable to set privilege {}in constructor", privilege);
            }
        }
    }

    /**
     * @brief Sets given privilege in the bitset
     *
     * @param[in] privilege  Privilege to be set
     *
     * @return               None
     *
     */
    bool setSinglePrivilege(std::string_view privilege)
    {
        for (size_t searchIndex = 0; searchIndex < privilegeNames.size();
             searchIndex++)
        {
            if (privilege == privilegeNames[searchIndex])
            {
                privilegeBitset.set(searchIndex);
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Resets the given privilege in the bitset
     *
     * @param[in] privilege  Privilege to be reset
     *
     * @return               None
     *
     */
    bool resetSinglePrivilege(const char* privilege)
    {
        for (size_t searchIndex = 0; searchIndex < privilegeNames.size();
             searchIndex++)
        {
            if (privilege == privilegeNames[searchIndex])
            {
                privilegeBitset.reset(searchIndex);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Retrieves names of all active privileges for a given type
     *
     * @param[in] type    Base or OEM
     *
     * @return            Vector of active privileges.  Pointers are valid until
     * the setSinglePrivilege is called, or the Privilege structure is destroyed
     *
     */
    std::vector<std::string>
        getActivePrivilegeNames(const PrivilegeType type) const
    {
        std::vector<std::string> activePrivileges;

        size_t searchIndex = 0;
        size_t endIndex = basePrivilegeCount;
        if (type == PrivilegeType::OEM)
        {
            searchIndex = basePrivilegeCount;
            endIndex = privilegeNames.size();
        }

        for (; searchIndex < endIndex; searchIndex++)
        {
            if (privilegeBitset.test(searchIndex))
            {
                activePrivileges.emplace_back(privilegeNames[searchIndex]);
            }
        }

        return activePrivileges;
    }

    /**
     * @brief Determines if this Privilege set is a superset of the given
     * privilege set
     *
     * @param[in] privilege  Privilege to be checked
     *
     * @return               None
     *
     */
    bool isSupersetOf(const Privileges& p) const
    {
        return (privilegeBitset & p.privilegeBitset) == p.privilegeBitset;
    }

    /**
     * @brief Returns the intersection of two Privilege sets.
     *
     * @param[in] privilege  Privilege set to intersect with.
     *
     * @return               The new Privilege set.
     *
     */
    Privileges intersection(const Privileges& p) const
    {
        return Privileges{privilegeBitset & p.privilegeBitset};
    }

  private:
    explicit Privileges(const std::bitset<maxPrivilegeCount>& p) :
        privilegeBitset{p}
    {}
    std::bitset<maxPrivilegeCount> privilegeBitset = 0;
};

inline Privileges getUserPrivileges(const persistent_data::UserSession& session)
{
    // default to no access
    Privileges privs;

    // Check if user is member of hostconsole group
    for (const auto& userGroup : session.userGroups)
    {
        if (userGroup == "hostconsole")
        {
            // Redfish privilege : host console access
            privs.setSinglePrivilege("OpenBMCHostConsole");
            break;
        }
    }

    if (session.userRole == "priv-admin")
    {
        // Redfish privilege : Administrator
        privs.setSinglePrivilege("Login");
        privs.setSinglePrivilege("ConfigureManager");
        privs.setSinglePrivilege("ConfigureSelf");
        privs.setSinglePrivilege("ConfigureUsers");
        privs.setSinglePrivilege("ConfigureComponents");
    }
    else if (session.userRole == "priv-operator")
    {
        // Redfish privilege : Operator
        privs.setSinglePrivilege("Login");
        privs.setSinglePrivilege("ConfigureSelf");
        privs.setSinglePrivilege("ConfigureComponents");
    }
    else if (session.userRole == "priv-user")
    {
        // Redfish privilege : Readonly
        privs.setSinglePrivilege("Login");
        privs.setSinglePrivilege("ConfigureSelf");
    }

    return privs;
}

/**
 * @brief The OperationMap represents the privileges required for a
 * single entity (URI).  It maps from the allowable verbs to the
 * privileges required to use that operation.
 *
 * This represents the Redfish "Privilege AND and OR syntax" as given
 * in the spec and shown in the Privilege Registry.  This does not
 * implement any Redfish property overrides, subordinate overrides, or
 * resource URI overrides.  This does not implement the limitation of
 * the ConfigureSelf privilege to operate only on your own account or
 * session.
 **/
using OperationMap = boost::container::flat_map<boost::beast::http::verb,
                                                std::vector<Privileges>>;

/* @brief Checks if user is allowed to call an operation
 *
 * @param[in] operationPrivilegesRequired   Privileges required
 * @param[in] userPrivileges                Privileges the user has
 *
 * @return                 True if operation is allowed, false otherwise
 */
inline bool isOperationAllowedWithPrivileges(
    const std::vector<Privileges>& operationPrivilegesRequired,
    const Privileges& userPrivileges)
{
    // If there are no privileges assigned, there are no privileges required
    if (operationPrivilegesRequired.empty())
    {
        return true;
    }
    for (const auto& requiredPrivileges : operationPrivilegesRequired)
    {
        BMCWEB_LOG_DEBUG("Checking operation privileges...");
        if (userPrivileges.isSupersetOf(requiredPrivileges))
        {
            BMCWEB_LOG_DEBUG("...success");
            return true;
        }
    }
    return false;
}

/**
 * @brief Checks if given privileges allow to call an HTTP method
 *
 * @param[in] method       HTTP method
 * @param[in] user         Privileges
 *
 * @return                 True if method allowed, false otherwise
 *
 */
inline bool isMethodAllowedWithPrivileges(const boost::beast::http::verb method,
                                          const OperationMap& operationMap,
                                          const Privileges& userPrivileges)
{
    const auto& it = operationMap.find(method);
    if (it == operationMap.end())
    {
        return false;
    }

    return isOperationAllowedWithPrivileges(it->second, userPrivileges);
}

} // namespace redfish
