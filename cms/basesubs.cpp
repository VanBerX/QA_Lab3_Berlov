#include <time.h>
#include <scriptpp/cmd.hpp>
#include <scriptpp/scrsort.hpp>

#include "_version.h"
#include "imgsize.h"
#include "urlenc.hpp"

#include "basesubs.hpp"


///////////////////////////////////////////////////////////
// class headers
//

class IfForm : public ScriptMacroprocessorMacro {
public:
    IfForm() : ScriptMacroprocessorMacro("if") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class OrForm : public ScriptMacroprocessorMacro {
public:
    OrForm() : ScriptMacroprocessorMacro("or") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class IfEqForm : public ScriptMacroprocessorMacro {
public:
    IfEqForm() : ScriptMacroprocessorMacro("ifeq") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class IfBelongs : public ScriptMacroprocessorMacro {
public:
    IfBelongs() : ScriptMacroprocessorMacro("ifbelongs") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class SwitchForm : public ScriptMacroprocessorMacro {
public:
    SwitchForm() : ScriptMacroprocessorMacro("switch") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};


class ForEach : public ScriptMacroprocessorMacro {
    ScriptMacroprocessor *the_master;
public:
    ForEach(ScriptMacroprocessor *m)
        : ScriptMacroprocessorMacro("foreach"), the_master(m) {}
    ScriptVariable Expand(const ScriptVector &params) const;
};


class ImgFileDimensions : public ScriptMacroprocessorMacro {
    ScriptVariable basepath;
public:
    ImgFileDimensions(const ScriptVariable &bp)
        : ScriptMacroprocessorMacro("imgdim"), basepath(bp)
    {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class IfFile : public ScriptMacroprocessorMacro {
    ScriptVariable basepath;
public:
    IfFile(const ScriptVariable &bp)
        : ScriptMacroprocessorMacro("iffile"), basepath(bp) {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class FileSize : public ScriptMacroprocessorMacro {
    ScriptVariable basepath;
public:
    FileSize(const ScriptVariable &bp)
        : ScriptMacroprocessorMacro("filesize"), basepath(bp) {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class ReadFile : public ScriptMacroprocessorMacro {
    ScriptVariable basepath;
public:
    ReadFile(const ScriptVariable &bp)
        : ScriptMacroprocessorMacro("readfile"), basepath(bp) {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class ListDir : public ScriptMacroprocessorMacro {
    ScriptVariable basepath;
public:
    ListDir(const ScriptVariable &bp)
        : ScriptMacroprocessorMacro("dir"), basepath(bp) {}
    ScriptVariable Expand(const ScriptVector &params) const;
};



class AttributeQuote : public ScriptMacroprocessorMacro {
public:
    AttributeQuote() : ScriptMacroprocessorMacro("q") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class RemoveLF : public ScriptMacroprocessorMacro {
public:
    RemoveLF() : ScriptMacroprocessorMacro("rmlf") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class CollapseWS : public ScriptMacroprocessorMacro {
public:
    CollapseWS() : ScriptMacroprocessorMacro("collapsews") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class Trim : public ScriptMacroprocessorMacro {
public:
    Trim() : ScriptMacroprocessorMacro("trim") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class SubstitutionURLEnc : public ScriptMacroprocessorMacro {
public:
    SubstitutionURLEnc() : ScriptMacroprocessorMacro("urlenc") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class SubstitutionLtGt : public ScriptMacroprocessorMacro {
public:
    SubstitutionLtGt() : ScriptMacroprocessorMacro("ltgt") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class Lhead : public ScriptMacroprocessorMacro {
public:
    Lhead() : ScriptMacroprocessorMacro("lhead") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class Ltail : public ScriptMacroprocessorMacro {
public:
    Ltail() : ScriptMacroprocessorMacro("ltail") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class Lindex : public ScriptMacroprocessorMacro {
public:
    Lindex() : ScriptMacroprocessorMacro("lindex") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class Lsort : public ScriptMacroprocessorMacro {
public:
    Lsort() : ScriptMacroprocessorMacro("lsort") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};



class MacroRFC2822Date : public ScriptMacroprocessorMacro {
public:
    MacroRFC2822Date() : ScriptMacroprocessorMacro("rfcdate") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class NowTime : public ScriptMacroprocessorMacro {
public:
    NowTime() : ScriptMacroprocessorMacro("now") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class ThalassaVersion : public ScriptMacroprocessorMacro {
public:
    ThalassaVersion() : ScriptMacroprocessorMacro("thalassa_version") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};



///////////////////////////////////////////////////////////////
// implementations

ScriptVariable IfForm::Expand(const ScriptVector &params) const
{
    // %[if:cond:then:else]
    ScriptVariable p0 = params[0];
    bool b = p0.IsValid() && p0.Trim() != "";
    ScriptVariable res = b ? params[1] : params[2];
    if(res.IsInvalid())
        res = "";
    return res;
}

ScriptVariable OrForm::Expand(const ScriptVector &params) const
{
    // %[or:alt1:alt2:...]
    int pc = params.Length();
    int i;
    for(i = 0; i < pc; i++) {
        ScriptVariable par = params[i];
        if(par.IsValid() && par.Trim().Length() > 0)
            return params[i];
    }
    return "";
}

ScriptVariable IfEqForm::Expand(const ScriptVector &params) const
{
    // %[ifeq:a:b:then:else]
    ScriptVariable p0 = params[0];
    ScriptVariable p1 = params[1];
    p0.Trim();
    p1.Trim();
    ScriptVariable res = (p0 == p1) ? params[2] : params[3];
    if(res.IsInvalid())
        res = "";
    return res;
}

ScriptVariable IfBelongs::Expand(const ScriptVector &params) const
{
    // %[ifbelongs:x:a b x c d:then:else]
    if(params.Length() < 3)
        return ScriptVariableInv();
    ScriptVariable p0 = params[0];
    p0.Trim();
    ScriptWordVector v(params[1]);
    bool found = false;
    int i;
    for(i = 0; i < v.Length(); i++)
        if(v[i] == p0) {
            found = true;
            break;
        }
    ScriptVariable res = found ? params[2] : params[3];
    if(res.IsInvalid())
        res = "";
    return res;
}

ScriptVariable IfCond::Expand(const ScriptVector &params) const
{
    // %[ifcond:then:else]
    ScriptVariable res = cond ? params[0] : params[1];
    if(res.IsInvalid())
        res = "";
    return res;
}

ScriptVariable ForEach::Expand(const ScriptVector &params) const
{
    // %[foreach:list:func:param1:..:paramN]

    int paramlen = params.Length();
    if(paramlen < 2)
        return ScriptVariableInv();

    int i;
    ScriptVector args;
    for(i = 2; i < paramlen; i++) {
        ScriptVariable p = params[i];
        p.Trim();
        args.AddItem(p);
    }

    ScriptVariable p1 = params[1];
    p1.Trim();

    int the_idx = args.Length();

    ScriptVariable res;
    ScriptWordVector lst(params[0]);
    int len = lst.Length();
    for(i = 0; i < len; i++) {
        args[the_idx] = lst[i];
        ScriptVariable m = the_master->Apply(p1, args);
        if(m.IsValid())
            res += m;
    }
    return res;
}

ScriptVariable SwitchForm::Expand(const ScriptVector &params) const
{
    // %[switch:expr:key1:val1:key2:val2:...]

    int paramlen = params.Length();
    if(paramlen < 2)
        return ScriptVariableInv();

    ScriptVariable expr = params[0];
    expr.Trim();

    int i;
    for(i = 1; i < paramlen-1; i += 2) {
        ScriptVariable v = params[i];
        v.Trim();
        if(v == expr)
            return params[i+1];
    }
    return "";
}


ScriptVariable IfFile::Expand(const ScriptVector &params) const
{
    if(params.Length() < 2)
        return ScriptVariableInv();
    ScriptVariable p0 = params[0];
    p0.Trim();
    ScriptVariable fname;
    if(p0[0] == '/') {
        fname = p0;
    } else {
        if(basepath.Length() > 0)
            fname = basepath + "/" + p0;
        else
            fname = p0;
    }
        // %[iffile:filename:then:else]
    FileStat fs(fname.c_str());
    ScriptVariable res = fs.Exists() ? params[1] : params[2];
    if(res.IsInvalid())
        res = "";
    return res;
}

ScriptVariable FileSize::Expand(const ScriptVector &params) const
{
    if(params.Length() < 1)
        return ScriptVariableInv();
    ScriptVariable p0 = params[0];
    p0.Trim();
    ScriptVariable fname;
    if(p0[0] == '/') {
        fname = p0;
    } else {
        if(basepath.Length() > 0)
            fname = basepath + "/" + p0;
        else
            fname = p0;
    }
    FileStat fs(fname.c_str());
    if(!fs.Exists() || !fs.IsRegularFile())
        return "";
    return ScriptNumber(fs.GetSize());
}

ScriptVariable ReadFile::Expand(const ScriptVector &params) const
{
    if(params.Length() < 1)
        return ScriptVariableInv();
    ScriptVariable p0 = params[0];
    p0.Trim();
    if(p0 == "")
        return "";
    ScriptVariable fname;
    if(p0[0] == '/') {
        fname = p0;
    } else {
        if(basepath.Length() > 0)
            fname = basepath + "/" + p0;
        else
            fname = p0;
    }
    ReadText rt(fname.c_str());
    if(!rt.IsOpen())
        return "";
    ScriptVariable res;
    rt.ReadUntilEof(res);
    return res;
}

static bool any_whitespace(const char *s)
{
    const char *p;
    for(p = s; *p; p++)
        if(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            return true;
    return false;
}

ScriptVariable ListDir::Expand(const ScriptVector &params) const
{
    if(params.Length() < 1)
        return ScriptVariableInv();
    ScriptVariable p0 = params[0];
    p0.Trim();
    if(p0 == "")
        return "";
    ScriptVariable dname;
    if(p0[0] == '/') {
        dname = p0;
    } else {
        if(basepath.Length() > 0)
            dname = basepath + "/" + p0;
        else
            dname = p0;
    }

    bool show_hidden = false, show_underscored = false;
    char must_start = 0;
    if(params.Length() >= 2) {
        ScriptVariable p1 = params[1];
        if(p1.Strchr('H').IsValid()) {
            must_start = '.';
        } else
        if(p1.Strchr('U').IsValid()) {
            must_start = '_';
        } else {
            if(p1.Strchr('h').IsValid())
                show_hidden = true;
            if(p1.Strchr('u').IsValid())
                show_underscored = true;
        }
    }

    ScriptVariable res("");
    ReadDir rd(dname.c_str());
    const char *nm;
    while((nm = rd.Next())) {
        if(any_whitespace(nm))
            continue;
        if(must_start && must_start != *nm)
            continue;
        if(*nm == '.' && !show_hidden)
            continue;
        if(*nm == '_' && !show_underscored)
            continue;
        if(res != "")
            res += " ";
        res += nm;
    }
    return res;
}

ScriptVariable ImgFileDimensions::Expand(const ScriptVector &params) const
{
    if(params.Length() < 1)
        return ScriptVariableInv();
    ScriptVariable p0 = params[0];
    p0.Trim();
    ScriptVariable fname;
    if(p0[0] == '/') {
        fname = p0;
    } else {
        if(basepath.Length() > 0)
            fname = basepath + "/" + p0;
        else
            fname = p0;
    }
    int w, h, res;
    res = extract_image_dimensions(fname.c_str(), &w, &h);
    if(res)
        return ScriptVariable(0, "width=\"%d\" height=\"%d\"", w, h);
    else
        return "";
}


ScriptVariable AttributeQuote::Expand(const ScriptVector &params) const
{
    bool has_sq = false, has_dq = false;
    const char *p;
    for(p = params[0].c_str(); *p; p++) {
        switch(*p) {
        case '\'':
            has_sq = true;
            if(has_dq)
                goto ready;
        case '\"':
            has_dq = true;
            if(has_sq)
                goto ready;
        }
    }
ready:
    if(!has_dq)
        return ScriptVariable("\"") + params[0] + "\"";
    // from now on, we know for sure ``"'' is there, so we need to trick it
    if(!has_sq)
        return ScriptVariable("\'") + params[0] + "\'";
    // the worst possible case -- they both are there; we leave the
    // surrounding dq's and replace every dq inside with ``&quot;''
    ScriptVariable res("\"");
    for(p = params[0].c_str(); *p; p++)
        if(*p == '\"')
            res += "&quot;";
        else
            res += *p;
    res += '\"';
    return res;
}

ScriptVariable RemoveLF::Expand(const ScriptVector &params) const
{
    ScriptVariable res("");
    const char *p;
    for(p = params[0].c_str(); *p; p++)
        if(*p != '\r' && *p != '\n')
            res += *p;
    return res;
}

ScriptVariable CollapseWS::Expand(const ScriptVector &params) const
{
    ScriptVariable res("");
    const char *p;
    bool spc = true;
    for(p = params[0].c_str(); *p; p++) {
        if(*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r') {
            if(!spc)
                res += ' ';
            spc = true;
        } else {
            res += *p;
            spc = false;
        }
    }
    return res;
}

ScriptVariable Trim::Expand(const ScriptVector &params) const
{
    ScriptVariable s = params[0];
    s.Trim();
    return s;
}

ScriptVariable SubstitutionURLEnc::Expand(const ScriptVector &params) const
{
    if(params.Length() != 1 || params[0].IsInvalid())
        return ScriptVariableInv();
    return url_encode(params[0]);
}

ScriptVariable SubstitutionLtGt::Expand(const ScriptVector &params) const
{
    if(params.Length() != 1)
        return ScriptVariableInv();

    ScriptVariable res("");
    const char *src = params[0].c_str();
    for(; *src; src++) {
        switch(*src) {
        case '<':
            res += "&lt;";
            break;
        case '>':
            res += "&gt;";
            break;
        case '&':
            res += "&amp;";
            break;
        // symbols <<">> and <<'>> are considered special by
        // JS function htmlspecialchars().
        // we don't understand the reason so we don't convert them here
        // we might add them here as well (" -> &quot;  ' -> &#039;)
        // if we experience any problems with them one day
        default:
            res += *src;
        }
    }
    return res;
}

ScriptVariable Lhead::Expand(const ScriptVector &params) const
{
    //  <string>   [<delim>]
    if(params.Length() < 1)
        return ScriptVariableInv();
    ScriptVariable delim("");
    if(params.Length() >= 2) {
        delim = params[1];
        delim.Trim();
    }
    ScriptVariable p0 = params[0];
    ScriptVariable::Substring w;
    if(delim == "") {
        p0.Whole().FetchWord(w);
    } else {
        w = p0.Strstr(delim);
        if(w.IsValid())
            w = w.Before();
        else
            w = p0.Whole();
    }
    return w.Get();
}

ScriptVariable Ltail::Expand(const ScriptVector &params) const
{
    if(params.Length() < 1)
        return ScriptVariableInv();
    ScriptVariable delim("");
    if(params.Length() >= 2) {
        delim = params[1];
        delim.Trim();
    }
    ScriptVariable p0 = params[0];
    ScriptVariable::Substring w;
    if(delim == "") {
        p0.Whole().FetchWord(w);
        w = w.After();
    } else {
        w = p0.Strstr(delim);
        if(w.IsValid())
            w = w.After();
        else
            w = p0.Whole();
    }
    return w.Get();
}

static void extract_delimiters(const ScriptVector &params, int delim_idx,
                       ScriptVariable &delimarg, ScriptVariable &trimarg)
{
    delimarg = " \t\r\n";
    trimarg.Invalidate();
    if(params.Length() < delim_idx)   // delimiters not specified
        return;

    const char *p = params[delim_idx].c_str();

    if(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            // special cases
        ScriptVariable dp = params[delim_idx];
        dp.Trim();
        if(dp == "n") {
            delimarg = "\n";
            trimarg = "\r";
            return;
        }
        if(dp == "N") {
            delimarg = "\n";
            trimarg = " \t\r";
            return;
        }
        // this behaviour is "unspecified" in the documentation
        // we leave the string untokenized, hopefully it's the most unexpected
        delimarg = "";
        trimarg = "";
        return;
    }

    ScriptVariable delimchars("");
    bool ws = false;
    for(; *p; p++) {
        if(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            ws = true;
        else
            delimchars += *p;
    }
    if(delimchars.Length() > 0) {  // argument was not ``empty''
        delimarg = delimchars;
        trimarg = ws ? " \t\r\n" : "";
    }
}

ScriptVariable Lindex::Expand(const ScriptVector &params) const
{
    //  <string>    <indices>    [<delim>]

    if(params.Length() < 2)
        return ScriptVariableInv();

    ScriptVariable delimarg, trimarg;
    extract_delimiters(params, 2, delimarg, trimarg);

    ScriptVector vec(params[0], delimarg.c_str(),
                                trimarg.IsValid() ? trimarg.c_str() : 0);
    ScriptVariable res("");
    char esc = 0;
    ScriptVariable p1 = params[1];
    p1.Trim();
    const char *src = p1.c_str();
    if(*src < '0' || *src > '9') {
        esc = *src;
        src++;
    }
    bool escaped = false;
    for(; *src; src++) {
        if(esc) {
            if(esc == *src) {
                escaped = true;
                continue;
            }
            if(escaped) {
                if(*src >= '0' && *src <= '9') {
                    int idx = *src - '0';
                    res += vec[idx];
                    escaped = false;
                    continue;
                }
                // current char will be added later
                escaped = false;
            }
        } else {
            if(*src >= '0' && *src <= '9') {
                int idx = *src - '0';
                res += vec[idx];
                continue;
            }
        }
        res += *src;
    }
    return res;
}

ScriptVariable Lsort::Expand(const ScriptVector &params) const
{
    //  <string>  [<delim>  [<glue-string>] ]

    if(params.Length() < 1)
        return ScriptVariableInv();

    ScriptVariable delimarg, trimarg;
    extract_delimiters(params, 1, delimarg, trimarg);

    ScriptVector vec(params[0], delimarg.c_str(),
                                trimarg.IsValid() ? trimarg.c_str() : 0);
    scriptpp_sort(vec);

    ScriptVariable glue(" ");
    if(params.Length() >= 3)
        glue = params[2];

    return vec.Join(glue);
}


ScriptVariable MacroRFC2822Date::Expand(const ScriptVector &params) const
{
    static const char * const monthname[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    if(params.Length() != 1 || params[0].IsInvalid())
        return ScriptVariableInv();
    ScriptVariable v = params[0];
    v.Trim();
    long long ln;
    if(!v.GetLongLong(ln, 10))
        return ScriptVariableInv();
    time_t tt = ln;
    struct tm *t = gmtime(&tt);
    int mx = t->tm_mon;
    if(mx < 0 || mx >= 12)     // must never happen unless gmtime is broken
        return ScriptVariableInv();
    ScriptVariable res(64, "%d %s %04d %02d:%02d:%02d +0000",
                       t->tm_mday, monthname[mx], t->tm_year + 1900,
                       t->tm_hour, t->tm_min, t->tm_sec);
    return res;
}

ScriptVariable NowTime::Expand(const ScriptVector &params) const
{
    return ScriptNumber(time(0));
}


ScriptVariable ThalassaVersion::Expand(const ScriptVector &params) const
{
    ScriptVariable s("");
    if(params.Length() > 0) {
        s = params[0];
        s.Trim();
    }
    if(s == "")
        return THALASSA_VERSION;
    if(s == "full")
        return "Thalassa CMS v. " THALASSA_VERSION " (built " __DATE__ ")";
    if(s == "id")
        return ScriptNumber(THALASSA_VERSIONID);
    if(s == "built")
        return __DATE__;
    return "";
}




BaseSubstitutions::BaseSubstitutions(const char *bp)
{
    AddMacro(new IfForm);                            // [if: ]
    AddMacro(new OrForm);                            // [or: ]
    AddMacro(new IfEqForm);                          // [ifeq: ]
    AddMacro(new IfBelongs);                         // [ifbelongs: ]
    AddMacro(new ForEach(this));                     // [foreach: ]
    AddMacro(new SwitchForm);                        // [switch: ]
    AddMacro(new AttributeQuote);                    // [q: ]
    AddMacro(new RemoveLF);                          // [rmlf: ]
    AddMacro(new CollapseWS);                        // [collapsews: ]
    AddMacro(new Trim);                              // [trim: ]
    AddMacro(new SubstitutionLtGt);                  // [ltgt: ]
    AddMacro(new SubstitutionURLEnc);                // [urlenc: ]
    AddMacro(new Lhead);                             // [lhead: ]
    AddMacro(new Ltail);                             // [ltail: ]
    AddMacro(new Lindex);                            // [lindex: ]
    AddMacro(new Lsort);                             // [lsort: ]
    AddMacro(new IfFile(bp));                        // [iffile: ]
    AddMacro(new FileSize(bp));                      // [filesize: ]
    AddMacro(new ReadFile(bp));                      // [readfile: ]
    AddMacro(new ListDir(bp));                       // [dir: ]
    AddMacro(new ImgFileDimensions(bp));             // [imgdim: ]
    AddMacro(new MacroRFC2822Date);                  // [rfcdate: ]
    AddMacro(new NowTime);                           // [now]
    AddMacro(new ThalassaVersion);                   // [thalassa_version: ]
}

BaseSubstitutions::~BaseSubstitutions()
{
}

