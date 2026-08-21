function rgb(x)
    return ((x & 0xFF) << 16) | (x & 0xFF00) | ((x & 0xFF0000) >> 16)
end

function UpdateTheme()
    theme = {
        dark = theme_dark_mode,
        default_fg = theme_default_fg,
        default_bg = theme_default_bg,
        comment = theme_comment,
        string = theme_string,
        number = theme_number,
        keyword = theme_keyword,
        control_flow = theme_control_flow,
        func = theme_function,
        type = theme_type,
        variable = theme_variable,
        constant = theme_constant,
        tag = theme_tag,
        attribute = theme_attribute,
        error = theme_error,
    }
end

UpdateTheme()

function DetectLanguageFromContents(contents)
    for name, L in pairs(languages) do
        if L.first_line then
            for _, pattern in ipairs(L.first_line) do
                if string.match(contents, pattern) then
                    return name
                end
            end
        end
    end
    return "Text"
end

function FilterForLanguage(name)
    local extensions = {}
    local language_definition = languages[name]

    if not language_definition.extensions then
        return nil
    end

    for _, ext in ipairs(language_definition.extensions) do
        if #ext > 0 then
            extensions[#extensions + 1] = "*." .. ext
        end
    end

    return  name .. " Files (" .. table.concat(extensions, " ") .. ")"
end

function DialogFilters()
    local filters = {}

    for name, L in pairs(languages) do
        local filter = FilterForLanguage(name)
        if filter then
            filters[#filters + 1] = filter
        end
    end

    table.sort(filters, function (a, b) return a:lower() < b:lower() end)
    table.insert(filters, 1, "All Files (*)")

    return table.concat(filters, ";;")
end

local function themedForeground(styleName, fallback)
    if not theme.dark then return fallback end

    local name = string.upper(tostring(styleName))
        if string.find(name, "ERROR", 1, true) or string.find(name, "ILLEGAL", 1, true)
            or string.find(name, "UNKNOWN", 1, true) then
            return theme.error
        end
    if string.find(name, "COMMENT", 1, true) then return theme.comment end
    if string.find(name, "STRING", 1, true) or string.find(name, "CHARACTER", 1, true)
        or string.find(name, "VERBATIM", 1, true) or string.find(name, "REGEX", 1, true) then
        return theme.string
    end
    if string.find(name, "NUMBER", 1, true) or string.find(name, "NUMERIC", 1, true)
        or string.find(name, "HEX", 1, true) or string.find(name, "BIN", 1, true) then
        return theme.number
    end
    if string.find(name, "FUNCTION", 1, true) or string.find(name, "METHOD", 1, true)
        or string.find(name, "DEFNAME", 1, true) or string.find(name, "CMDLET", 1, true) then
        return theme.func
    end
    if string.find(name, "CLASS", 1, true) or string.find(name, "TYPE", 1, true) then
        return theme.type
    end
    if string.find(name, "ATTRIBUTE", 1, true) or string.find(name, "PROPERTY", 1, true) then
        return theme.attribute
    end
    if string.find(name, "TAG", 1, true) then return theme.tag end
    if string.find(name, "VARIABLE", 1, true) or string.find(name, "PARAM", 1, true) then
        return theme.variable
    end
    if string.find(name, "CONSTANT", 1, true) or string.find(name, "MACRO", 1, true) then
        return theme.constant
        end
        if string.find(name, "LABEL", 1, true) or string.find(name, "PREPROCESSOR", 1, true) then
            return theme.constant
    end
    if string.find(name, "CONTROL", 1, true) then return theme.control_flow end
    if string.find(name, "KEYWORD", 1, true) or string.find(name, "INSTRUCTION", 1, true)
        or string.find(name, "COMMAND", 1, true) or string.find(name, "DIRECTIVE", 1, true)
        or string.find(name, "RESERVED", 1, true) then
        return theme.keyword
    end
    if string.find(name, "DEFAULT", 1, true) or string.find(name, "WHITE", 1, true)
        or string.find(name, "IDENTIFIER", 1, true) or string.find(name, "OPERATOR", 1, true)
        or string.find(name, "PUNCTUATION", 1, true) then
        return theme.default_fg
    end
    if fallback == rgb(0x000000) then return theme.default_fg end
    return fallback
end

local function SetStyleAppearance(L)
    if L.styles then
        for styleName, style in pairs(L.styles) do
            editor.StyleFore[style.id] = themedForeground(styleName, style.fgColor)
            editor.StyleBack[style.id] = theme.dark and theme.default_bg or style.bgColor

            if style.fontStyle then
                editor.StyleBold[style.id] = (style.fontStyle & 1 == 1)
                editor.StyleItalic[style.id] = (style.fontStyle & 2 == 2)
                editor.StyleUnderline[style.id] = (style.fontStyle & 4 == 4)
                editor.StyleEOLFilled[style.id] = (style.fontStyle & 8 == 8)
            end
        end
    end
end

function SetStyle(L)
    SetStyleAppearance(L)

    if L.keywords then
        for id, kw in pairs(L.keywords) do
            editor.KeyWords[id] = kw
        end
    end

    if L.properties then
        for p, v in pairs(L.properties) do
            editor.Property[p] = v
        end
    end
end

function SetLanguageAppearance(languageName)
    local L = languages[languageName]

    SetStyleAppearance(L)

    if L.additionalLanguages then
        for _, language in pairs(L.additionalLanguages) do
            SetStyleAppearance(languages[language])
        end
    end
end

function SetLanguage(languageName)
    local L = languages[languageName]

    if not skip_tabs then
        editor.UseTabs = (L.tabSettings or "tabs") == "tabs"
    end

    if not skip_tabwidth then
        editor.TabWidth = L.tabSize or 4
    end

    editor.MarginWidthN[2] = L.disableFoldMargin and 0 or 16

    SetStyle(L)

    if L.additionalLanguages then
        for _, language in pairs(L.additionalLanguages) do
            SetStyle(languages[language])
        end
    end


    editor.Property["fold"] = "1"
    editor.Property["fold.compact"] = "0"
end

function GetLanguageKeywords(languageName)
    local L = languages[languageName]

    if not L or not L.keywords then
        return {}
    end

    local seen = {}

    local function collectKeywords(lang)
        if not lang then return end
        if lang.keywords then
            for _, kwString in pairs(lang.keywords) do
                for token in kwString:gmatch("%S+") do
                    seen[token] = true
                end
            end
        end
    end

    collectKeywords(L)

    if L.additionalLanguages then
        for _, language in pairs(L.additionalLanguages) do
            collectKeywords(languages[language])
        end
    end

    local result = {}
    for token, _ in pairs(seen) do
        result[#result + 1] = token
    end

    table.sort(result)

    return result
end

languages = {}
languages["ActionScript"] = require("actionscript")
languages["ADA"] = require("ada")
languages["Assembly"] = require("asm")
languages["ASN.1"] = require("asn1")
languages["asp"] = require("asp")
languages["autoIt"] = require("autoit")
languages["AviSynth"] = require("avs")
languages["BaanC"] = require("baanc")
languages["bash"] = require("bash")
languages["Batch"] = require("batch")
languages["BlitzBasic"] = require("blitzbasic")
languages["C"] = require("c")
languages["Caml"] = require("caml")
languages["CMakeFile"] = require("cmake")
languages["COBOL"] = require("cobol")
languages["Csound"] = require("csound")
languages["CoffeeScript"] = require("coffeescript")
languages["C++"] = require("cpp")
languages["C#"] = require("cs")
languages["CSS"] = require("css")
languages["SCSS"] = require("scss")
languages["D"] = require("d")
languages["DIFF"] = require("diff")
languages["Erlang"] = require("erlang")
languages["ESCRIPT"] = require("escript")
languages["Forth"] = require("forth")
languages["Fortran (free form)"] = require("fortran")
languages["Fortran (fixed form)"] = require("fortran77")
languages["FreeBasic"] = require("freebasic")
languages["GUI4CLI"] = require("gui4cli")
languages["Go"] = require("go")
languages["Haskell"] = require("haskell")
languages["HTML"] = require("html")
languages["ini file"] = require("ini")
languages["InnoSetup"] = require("inno")
languages["Intel HEX"] = require("ihex")
languages["Java"] = require("java")
languages["JavaScript"] = require("javascript")
languages["JSON"] = require("json")
languages["KiXtart"] = require("kix")
languages["LISP"] = require("lisp")
languages["LaTeX"] = require("latex")
languages["Lua"] = require("lua")
languages["Less"] = require("less")
languages["Makefile"] = require("makefile")
languages["Markdown"] = require("markdown")
languages["Matlab"] = require("matlab")
languages["MMIXAL"] = require("mmixal")
languages["Nimrod"] = require("nimrod")
languages["Nix"] = require("nix")
languages["extended crontab"] = require("nncrontab")
languages["Dos Style"] = require("nfo")
languages["NSIS"] = require("nsis")
languages["OScript"] = require("oscript")
languages["Objective-C"] = require("objc")
languages["Pascal"] = require("pascal")
languages["Perl"] = require("perl")
languages["PHP"] = require("php")
languages["Postscript"] = require("postscript")
languages["PowerShell"] = require("powershell")
languages["Properties file"] = require("props")
languages["PureBasic"] = require("purebasic")
languages["Python"] = require("python")
languages["R"] = require("r")
languages["REBOL"] = require("rebol")
languages["registry"] = require("registry")
languages["RC"] = require("rc")
languages["Ruby"] = require("ruby")
languages["Rust"] = require("rust")
languages["Scheme"] = require("scheme")
languages["Smalltalk"] = require("smalltalk")
languages["spice"] = require("spice")
languages["SQL"] = require("sql")
languages["S-Record"] = require("srec")
languages["Swift"] = require("swift")
languages["TCL"] = require("tcl")
languages["Tektronix extended HEX"] = require("tehex")
languages["TeX"] = require("tex")
languages["Text"] = require("text")
languages["VB / VBS"] = require("vb")
languages["txt2tags"] = require("txt2tags")
languages["Verilog"] = require("verilog")
languages["VHDL"] = require("vhdl")
languages["Visual Prolog"] = require("visualprolog")
languages["XML"] = require("xml")
languages["YAML"] = require("yaml")
languages["Abaqus"] = require("abaqus")
