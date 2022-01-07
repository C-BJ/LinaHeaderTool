/*
This file is a part of: Lina Engine
https://github.com/inanevin/LinaEngine

Author: Inan Evin
http://www.inanevin.com

Copyright (c) [2018-2020] [Inan Evin]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "HeaderTool.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <filesystem>

#define ROOT_PATH         "../../"
#define REGISTRY_CPP_PATH "../../LinaEngine/src/Core/ReflectionRegistry.cpp"

int main(int argc, char** argv)
{
    Lina::HeaderTool tool;
    tool.Run(ROOT_PATH);
    tool.SerializeReadData();
    return 0;
}

namespace Lina
{

#define LINA_CLASS_MACRO             "LINA_CLASS("
#define LINA_COMPONENT_MACRO         "LINA_COMPONENT("
#define LINA_PROPERTY_MACRO          "LINA_PROPERTY("
#define REGISTER_FUNC_BGN_IDENTIFIER "//REGFUNC_BEGIN"
#define REGISTER_FUNC_END_IDENTIFIER "//REGFUNC_END"
#define INCLUDE_BGN_IDENTIFIER       "//INC_BEGIN"
#define INCLUDE_END_IDENTIFIER       "//INC_END"

    std::vector<std::string> excludePaths{
        ".vs",
        ".git",
        "build",
        "CMake",
        "Docs",
        "vendor",
        "VSItem",
        ".clang",
        ".travis",
        "LICENSE",
        "README",
        ".sln",
    };

    HeaderTool::~HeaderTool()
    {
        for (auto& p : m_componentData)
            delete p.second;
    }

    void HeaderTool::Run(const std::string& path)
    {
        // Scan each folder & sub-folders and find all .hpp files.
        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            if (entry.path().has_extension())
            {
                const std::string fullName  = entry.path().filename().string();
                const std::string extension = fullName.substr(fullName.find(".") + 1);

                // Skip the property declaration file.
                if (fullName.find("CommonReflection") == std::string::npos && extension.compare("hpp") == 0 || extension.compare("h") == 0)
                {
                    std::string replacedPath = entry.path().string();
                    std::replace(replacedPath.begin(), replacedPath.end(), '\\', '/');
                    std::string include = replacedPath.substr(replacedPath.find("include"));
                    m_lastHPPInclude    = include.substr(include.find_first_of("/") + 1);
                    ReadHPP(entry.path().string());
                }
            }
            else
            {

                std::string replacedPath = entry.path().string();

                bool shouldExclude = false;
                for (auto& excludeStr : excludePaths)
                {
                    if (replacedPath.find(excludeStr) != std::string::npos)
                    {
                        shouldExclude = true;
                        break;
                    }
                }

                if (!shouldExclude)
                {
                    std::replace(replacedPath.begin(), replacedPath.end(), '\\', '/');
                    Run(replacedPath);
                }
            }
        }
    }

    void HeaderTool::ReadHPP(const std::string& hpp)
    {
        std::ifstream file;
        file.open(hpp);
        std::string line;

        // Read the hpp line by line, find LINA_CLASS and LINA_PROPERTY macros.
        if (file.is_open())
        {
            while (file.good())
            {
                getline(file, line);
                if (nextLineIsComponent)
                {
                    std::string componentName = line.substr(0, line.find(":"));
                    RemoveWordFromLine(componentName, "class");
                    RemoveWordFromLine(componentName, "struct");
                    RemoveWhitespaces(componentName);

                    nextLineIsComponent                = false;
                    LinaComponent* linaComponent       = new LinaComponent();
                    *linaComponent                     = m_lastComponentData;
                    linaComponent->m_hppInclude        = m_lastHPPInclude;
                    linaComponent->m_nameWithNamespace = m_lastNamespace + "::" + componentName;
                    linaComponent->m_name              = componentName;
                    m_componentData[componentName]     = linaComponent;
                    m_namespaceComponentMap[m_lastNamespace].push_back(linaComponent);
                    m_lastClass              = componentName;
                    m_lastHeaderWasComponent = true;
                }
                else if (nextLineIsClass)
                {
                    std::string className = line.substr(0, line.find(":"));
                    RemoveWordFromLine(className, "class");
                    RemoveWordFromLine(className, "struct");
                    RemoveWhitespaces(className);

                    nextLineIsClass                = false;
                    LinaClass* linaClass           = new LinaClass();
                    *linaClass                     = m_lastClassData;
                    linaClass->m_hppInclude        = m_lastHPPInclude;
                    linaClass->m_nameWithNamespace = m_lastNamespace + "::" + className;
                    linaClass->m_name              = className;
                    m_classData[className]         = linaClass;
                    m_namespaceClassMap[m_lastNamespace].push_back(linaClass);
                    m_lastClass              = className;
                    m_lastHeaderWasComponent = false;
                }
                else if (nextLineIsProperty)
                {
                    // We have read the property macro, now we will read the property name
                    auto equals = line.find("=") != std::string::npos;

                    // If has equals sign, remove everything after the sign including the sign.
                    // If not, just remove the ;
                    if (equals)
                    {
                        auto equals = line.find("=");
                        line        = line.substr(0, equals);
                    }
                    else
                        line = line.substr(0, line.find(";"));

                    // Remove the last line until it's not a whitespace.
                    while (std::isspace(line.back()))
                        line = line.substr(0, line.length() - 1);

                    // Now the last character should be the last char of the variable's name
                    // Find the last whitespace, which should be the one right before the variable name
                    // and cut the string before that.
                    line = line.substr(line.find_last_of(' '));
                    RemoveWhitespaces(line);

                    m_lastProperty.m_propertyName = line;

                    if (m_lastHeaderWasComponent)
                        m_componentData[m_lastClass]->m_properties.push_back(m_lastProperty);
                    else
                        m_classData[m_lastClass]->m_properties.push_back(m_lastProperty);

                    nextLineIsProperty = false;
                }
                else
                {
                    if (line.find(LINA_COMPONENT_MACRO) != std::string::npos)
                    {
                        ProcessComponentMacro(line);
                        nextLineIsComponent = true;
                    }
                    else if (line.find(LINA_PROPERTY_MACRO) != std::string::npos)
                    {
                        ProcessPropertyMacro(line);
                        nextLineIsProperty = true;
                    }
                    else if (line.find(LINA_CLASS_MACRO) != std::string::npos)
                    {
                        ProcessClassMacro(line);
                        nextLineIsClass = true;
                    }
                    else if (line.find("namespace") != std::string::npos)
                    {
                        RemoveBrackets(line);
                        RemoveString(line, "namespace");
                        RemoveWhitespaces(line);

                        if (line.find("Lina::") != std::string::npos)
                            RemoveString(line, "Lina::");
                        m_lastNamespace = line;
                    }
                }
            }
        }
    }

    void HeaderTool::RemoveWordFromLine(std::string& line, const std::string& word)
    {
        auto n = line.find(word);
        if (n != std::string::npos)
        {
            line.erase(n, word.length());
        }
    }

    void HeaderTool::ProcessPropertyMacro(const std::string& line)
    {
        std::string trimmed           = line.substr(line.find("(") + 1);
        std::string insideParanthesis = trimmed.substr(0, trimmed.find(")"));

        const int itemCount = 4;
        for (int i = 0; i < itemCount; i++)
        {
            const size_t                               firstQuote = insideParanthesis.find("\"");
            const size_t                                secondQuote = insideParanthesis.find("\"", firstQuote+1);
            std::string property = insideParanthesis.substr(firstQuote + 1, secondQuote - 1 - firstQuote);
            RemoveWhitespacesPreAndPost(property);
            insideParanthesis = insideParanthesis.substr(secondQuote + 1);

            if (i == 0)
                m_lastProperty.m_title = property;
            else if (i == 1)
                m_lastProperty.m_type = property;
            else if (i == 2)
                m_lastProperty.m_tooltip = property;
            else if (i == 3)
                m_lastProperty.m_dependsOn = property;
        }
    }

    void HeaderTool::ProcessComponentMacro(const std::string& line)
    {
        std::string trimmed           = line.substr(line.find("(") + 1);
        std::string insideParanthesis = trimmed.substr(0, trimmed.find(")"));

        const int itemCount = 5;
        for (int i = 0; i < itemCount; i++)
        {
            const size_t firstQuote  = insideParanthesis.find("\"");
            const size_t secondQuote = insideParanthesis.find("\"", firstQuote + 1);
            std::string  property    = insideParanthesis.substr(firstQuote + 1, secondQuote - 1 - firstQuote);
            RemoveWhitespacesPreAndPost(property);
            insideParanthesis = insideParanthesis.substr(secondQuote + 1);

            if (i == 0)
                m_lastComponentData.m_title = property;
            else if (i == 1)
                m_lastComponentData.m_icon = property;
            else if (i == 2)
                m_lastComponentData.m_category = property;
            else if (i == 3)
                m_lastComponentData.m_canAddComponent = property.compare("true") == 0 ? true : false;
            else if (i == 4)
                m_lastComponentData.m_listenToValueChanged = property.compare("true") == 0 ? true : false;
        }
    }

    void HeaderTool::ProcessClassMacro(const std::string& line)
    {
        std::string trimmed           = line.substr(line.find("(") + 1);
        std::string insideParanthesis = trimmed.substr(0, trimmed.find(")"));

        const int itemCount = 1;
        for (int i = 0; i < itemCount; i++)
        {
            const size_t firstQuote  = insideParanthesis.find("\"");
            const size_t secondQuote = insideParanthesis.find("\"", firstQuote + 1);
            std::string  property    = insideParanthesis.substr(firstQuote + 1, secondQuote - 1 - firstQuote);
            RemoveWhitespacesPreAndPost(property);
            insideParanthesis = insideParanthesis.substr(secondQuote + 1);

            if (i == 0)
                m_lastClassData.m_title = property;
        }
    }

    void HeaderTool::RemoveWhitespaces(std::string& str)
    {
        std::string::iterator end_pos = std::remove(str.begin(), str.end(), ' ');
        str.erase(end_pos, str.end());
    }

    void HeaderTool::RemoveWhitespacesPreAndPost(std::string& str)
    {
        const size_t firstChar = str.find_first_not_of(' ');
        const size_t lastChar  = str.find_last_not_of(' ');

        if (str.size() > lastChar)
            str = str.substr(firstChar, lastChar + 1);
    }

    void HeaderTool::RemoveComma(std::string& str)
    {
        std::string::iterator end_pos = std::remove(str.begin(), str.end(), ',');
        str.erase(end_pos, str.end());
    }

    void HeaderTool::RemoveDoubleQuote(std::string& str)
    {
        std::string::iterator end_pos = std::remove(str.begin(), str.end(), '"');
        str.erase(end_pos, str.end());
    }

    void HeaderTool::RemoveString(std::string& str, const std::string& toErase)
    {
        size_t pos = str.find(toErase);
        str.erase(pos, toErase.length());
    }

    void HeaderTool::RemoveBrackets(std::string& str)
    {
        if (str.find("{") != std::string::npos)
        {
            std::string::iterator end_pos = std::remove(str.begin(), str.end(), '{');
            str.erase(end_pos, str.end());
        }

        if (str.find("}") != std::string::npos)
        {
            std::string::iterator end_pos = std::remove(str.begin(), str.end(), '}');
            str.erase(end_pos, str.end());
        }
    }
    void HeaderTool::SerializeReadData()
    {

        std::ifstream file;
        std::string   line;
        file.open(REGISTRY_CPP_PATH);
        bool                     registerFunctionFound = false;
        bool                     includeFound          = false;
        std::vector<std::string> fileContents;

        if (file.is_open())
        {
            while (file.good())
            {
                getline(file, line);

                if (registerFunctionFound)
                {
                    if (line.find(REGISTER_FUNC_END_IDENTIFIER) == std::string::npos)
                        continue;
                    else
                    {
                        registerFunctionFound = false;
                        fileContents.push_back(line);
                    }
                }
                else if (includeFound)
                {
                    if (line.find(INCLUDE_END_IDENTIFIER) == std::string::npos)
                        continue;
                    else
                    {
                        includeFound = false;
                        fileContents.push_back(line);
                    }
                }
                else
                    fileContents.push_back(line);

                if (line.find(INCLUDE_BGN_IDENTIFIER) != std::string::npos)
                {
                    includeFound = true;

                    for (auto& [actualName, compData] : m_componentData)
                        fileContents.push_back("#include \"" + compData->m_hppInclude + "\"");

                    for (auto& [actualName, classData] : m_classData)
                        fileContents.push_back("#include \"" + classData->m_hppInclude + "\"");
                }
                else if (line.find(REGISTER_FUNC_BGN_IDENTIFIER) != std::string::npos)
                {
                    registerFunctionFound = true;
                    for (auto& [actualName, componentData] : m_componentData)
                    {
                        const std::string        className = componentData->m_nameWithNamespace;
                        std::vector<std::string> functionCommands;
                        functionCommands.push_back("entt::meta<" + className + ">().func<&REF_CloneComponent<" + className + ">, entt::as_void_t>(\"clone\"_hs);");
                        functionCommands.push_back("entt::meta<" + className + ">().func<&REF_SerializeComponent<" + className + ">, entt::as_void_t>(\"serialize\"_hs);");
                        functionCommands.push_back("entt::meta<" + className + ">().func<&REF_DeserializeComponent<" + className + ">, entt::as_void_t>(\"deserialize\"_hs);");
                        functionCommands.push_back("entt::meta<" + className + ">().func<&REF_SetEnabled<" + className + ">, entt::as_void_t>(\"setEnabled\"_hs);");
                        functionCommands.push_back("entt::meta<" + className + ">().func<&REF_Get<" + className + ">, entt::as_ref_t>(\"get\"_hs);");
                        functionCommands.push_back("entt::meta<" + className + ">().func<&REF_Reset<" + className + ">, entt::as_void_t>(\"reset\"_hs);");
                        functionCommands.push_back("entt::meta<" + className + ">().func<&REF_Has<" + className + ">, entt::as_void_t>(\"has\"_hs);");
                        functionCommands.push_back("entt::meta<" + className + ">().func<&REF_Remove<" + className + ">, entt::as_void_t>(\"remove\"_hs);");
                        functionCommands.push_back("entt::meta<" + className + ">().func<&REF_Copy<" + className + ">, entt::as_void_t>(\"copy\"_hs);");
                        functionCommands.push_back("entt::meta<" + className + ">().func<&REF_Paste<" + className + ">, entt::as_void_t>(\"paste\"_hs);");

                        if (componentData->m_canAddComponent)
                            functionCommands.push_back("entt::meta<" + className + ">().func<&REF_Add<" + className + ">, entt::as_void_t>(\"add\"_hs);");

                        if (componentData->m_listenToValueChanged)
                            functionCommands.push_back("entt::meta<" + className + ">().func<&REF_ValueChanged<" + className + ">, entt::as_void_t>(\"add\"_hs);");

                        // Class meta.
                        fileContents.push_back("entt::meta<" + className + ">().type().props(std::make_pair(\"Title\"_hs, \"" + componentData->m_title + "\"), std::make_pair(\"Icon\"_hs," + componentData->m_icon + "), std::make_pair(\"Category\"_hs,\"" + componentData->m_category + "\"));");

                        // inherited m_isEnabled
                        fileContents.push_back("entt::meta<" + className + ">().data<&" + className + "::" + "m_isEnabled" + ">(\"m_isEnabled\"_hs);");

                        for (auto& property : componentData->m_properties)
                        {
                            const std::string propRegister = "entt::meta<" + className + ">().data<&" + className + "::" + property.m_propertyName + ">(\"" + property.m_propertyName + "\"_hs)";
                            const std::string dataRegister = ".props(std::make_pair(\"Title\"_hs,\"" + property.m_title + "\")," + "std::make_pair(\"Type\"_hs,\"" + property.m_type + "\")," + "std::make_pair(\"Tooltip\"_hs,\"" + property.m_tooltip + "\")," + "std::make_pair(\"Depends\"_hs,\"" + property.m_dependsOn + "\"_hs));";
                            fileContents.push_back(propRegister + dataRegister);
                        }

                        for (auto& fc : functionCommands)
                            fileContents.push_back(fc);
                    }

                    for (auto& [actualName, classData] : m_classData)
                    {
                        const std::string className = classData->m_nameWithNamespace;
                        fileContents.push_back("entt::meta<" + className + ">().type().props(\"Title\"_hs, \"" + classData->m_title + "\");");
                        for (auto& property : classData->m_properties)
                        {
                            const std::string propRegister = "entt::meta<" + className + ">().data<&" + className + "::" + property.m_propertyName + ">(\"" + property.m_propertyName + "\"_hs)";
                            const std::string dataRegister = ".props(std::make_pair(\"Title\"_hs,\"" + property.m_title + "\")," + "std::make_pair(\"Type\"_hs,\"" + property.m_type + "\")," + "std::make_pair(\"Tooltip\"_hs,\"" + property.m_tooltip + "\")," + "std::make_pair(\"Depends\"_hs,\"" + property.m_dependsOn + "\"_hs));";
                            fileContents.push_back(propRegister + dataRegister);
                        }
                    }
                }
            }
        }

        file.close();

        std::ofstream newFile;
        newFile.open(REGISTRY_CPP_PATH, std::ofstream::out | std::ofstream::trunc);

        for (int i = 0; i < fileContents.size(); i++)
            newFile << fileContents[i] << std::endl;
        newFile.close();
    }

} // namespace Lina
