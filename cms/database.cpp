#include <inifile/inifile.hpp>
#include <scriptpp/conffile.hpp>
#include <scriptpp/scrmsg.hpp>
#include <scriptpp/cmd.hpp>

#include <stdio.h>
#include <time.h>   // XXX for ctime -- remove once ctime is removed

enum { too_long_for_filename = 128 };


#include "dbsubst.hpp"
#include "filters.hpp"
#include "fpublish.hpp"   // for enum file_publish_methods
#include "arrindex.hpp"
#include "dbforum.hpp"
#include "forumgen.hpp"

#include "database.hpp"



static bool svec_has_elem(const ScriptVector &v, const ScriptVariable &el)
{
    int i;
    for(i = 0; i < v.Length(); i++) {
        if(v[i] == el)
            return true;
    }
    return false;
}


bool ListItemData::HasTag(const ScriptVariable &t) const
{
    return svec_has_elem(tags, t);
}

bool ListItemData::HasFlag(const ScriptVariable &f) const
{
    return svec_has_elem(flags, f);
}

bool CommentData::HasFlag(const ScriptVariable &f) const
{
    return svec_has_elem(flags, f);
}



/* We need this structure for the ``set''-based lists only  */
struct SetListIndex {
    ScriptVariable set_id, tag;
    ScriptVector items;
    SetListIndex *next;
};

/////////////////////////////////
// BlockGroups must be defined before the Database's destructor

struct BlockGroupData {
    ScriptVariable id;
    ScriptVariable begin, end, block_templ;
    struct BlockData *first;
    BlockGroupData *next_grp;
    BlockGroupData(const char *i, const char *b, const char *e,
                   const char *bt)
        : id(i), begin(b), end(e), block_templ(bt), first(0), next_grp(0) {}
    ~BlockGroupData();
};

struct BlockData {
    ScriptVariable id;
    int weight;
    ScriptVariable tag, title, body;
    BlockData *next;
    BlockData(const char *i,
              int w, const char *tg, const char *tit, const char *b)
        : id(i), weight(w), tag(tg), title(tit), body(b), next(0)
    {}
};

BlockGroupData::~BlockGroupData()
{
    if(next_grp)
        delete next_grp;
    while(first) {
        BlockData *tmp = first;
        first = first->next;
        delete tmp;
    }
}

/////////////////////////////////


Database::Database()
    : subst(0), filtmaker(0), current_list_data(0), current_list_item_data(0),
    first_block_group(0), first_set_list(0)
{
    inifile = new IniFileParser;
    // subst object is to be created after loading the inifile,
    // because some of the "variables" are initialized by calling
    // methods of the database, so these methods must already
    // be prepared to return their respective values
}

Database::~Database()
{
    if(subst)
        delete subst;
    delete inifile;
    if(filtmaker)
        delete filtmaker;
    while(first_set_list) {
        SetListIndex *tmp = first_set_list;
        first_set_list = first_set_list->next;
        delete tmp;
    }
    if(first_block_group)
        delete first_block_group;
}

void Database::SetOptSelector(const ScriptVariable &whatfor,
                              const ScriptVariable &sel)
{
    if(whatfor.IsInvalid() || whatfor == "") {
        default_opt_selector = sel;
        return;
    }
    int i;
    for(i = 0; i+1 < opt_selectors.Length(); i += 2) {
        if(opt_selectors[i] == whatfor) {
            opt_selectors[i+1] = sel;
            return;
        }
    }
    opt_selectors.AddItem(whatfor);
    opt_selectors.AddItem(sel);
}

bool Database::Load(const char *filename)
{
    bool ok = inifile->Load(filename);
    if(subst)
        delete subst;
    subst = new DatabaseSubstitution(this);
    if(default_opt_selector.IsValid())
        inifile->SetTextParameter("general", 0, "opt_selector",
                                  default_opt_selector.c_str());
    return ok;
}

void Database::MakeErrorMessage(class ScriptVariable &msg) const
{
    msg = "";
    int el = inifile->GetLastErrorLine();
    if(el != -1)
        msg += ScriptNumber(el) + ": ";
    else
        msg += " ";
    msg += inifile->GetLastErrorDescription();
}

bool Database::GetExtraFiles(ScriptVector &ef)
{
    const char *tmp = inifile->GetTextParameter("general", 0, "inifiles", 0);
    if(!tmp)
        return false;
    ScriptVector v((*subst)(tmp), " ,\t\r\n");
    if(v.Length() < 1)
        return false;
    ef = v;
    inifile->SetTextParameter("general", 0, "inifiles", "");
    return true;
}

ScriptVariable Database::GetHtmlSnippet(const ScriptVariable &name) const
{
    return inifile->GetTextParameter("html", 0, name.c_str(), "");
}

ScriptVariable Database::BuildGenericPart(const ScriptVariable &templ) const
{
    return (*subst)(templ);
}

#if 0
ScriptVariable Database::GetSourcePrefix() const
{
    if(source_prefix.IsInvalid())
        const_cast<Database*>(this)->source_prefix =
            inifile->GetTextParameter("general", 0, "srcdir", "");
    return source_prefix;
}
#endif

ScriptVariable Database::GetFilePrefix() const
{
    if(file_prefix.IsInvalid()) {
        ScriptVariable fp =
            inifile->GetTextParameter("general", 0, "rootdir", "html");
        fp = (*subst)(fp);
        if(fp.Range(-1,1)[0] != '/')
            fp += '/';
        const_cast<Database*>(this)->file_prefix = fp;
    }
    return file_prefix;
}

ScriptVariable Database::GetSpoolDir() const
{
    return (*subst)(inifile->
                    GetTextParameter("general", 0, "spooldir", "_spool"));
}

void Database::SetFilePrefix(ScriptVariable s)
{
    file_prefix = (s.Range(-1,1)[0] == '/') ? s : s + '/';
}

ScriptVariable Database::GetBaseUri() const
{
    if(base_uri.IsInvalid()) {
        const char *tmp =
            inifile->GetTextParameter("general", 0, "base_uri", "/");
        if(tmp)
            const_cast<Database*>(this)->base_uri = (*subst)(tmp);
    }
    return base_uri;
}

ScriptVariable Database::GetBaseUrl() const
{
    return (*subst)(inifile->GetTextParameter("general", 0, "base_url", ""));
}

ScriptVariable Database::GetOption(const ScriptVariable &section,
                                   const ScriptVariable &name) const
{
    const char *tmp;

    int sel_idx = -1;
    int i;
    for(i = 0; i+1 < opt_selectors.Length(); i += 2) {
        if(opt_selectors[i] == section) {
            sel_idx = i + 1;
            break;
        }
    }
    if(sel_idx == -1) {
        tmp = inifile->GetModifiedTextParameter("general", 0,
                           "opt_selector", section.c_str(), "");
        ScriptVector &opsel = const_cast<Database*>(this)->opt_selectors;
        opsel.AddItem(section);
        opsel.AddItem(tmp);
        sel_idx = opsel.Length() - 1;  // the last, which is just added
    }
        // NB: now sel_idx is valid, as well as opt_selectors[sel_idx]

    const char *sstr = section.c_str();
    const char *nstr = name.c_str();
    const char *selstr = opt_selectors[sel_idx].c_str();
    tmp = selstr && *selstr ?
        inifile->GetModifiedTextParameter("options", sstr, nstr, selstr, 0)
        :
        inifile->GetTextParameter("options", sstr, nstr, 0);
    if(!tmp)
        return ScriptVariableInv();
    return tmp;
}

int Database::GetGenfiles(class ScriptVector &names) const
{
    return GetSectionNames("genfile", names);
}

int Database::GetPages(class ScriptVector &names) const
{
    return GetSectionNames("page", names);
}

int Database::GetLists(class ScriptVector &names) const
{
    return GetSectionNames("list", names);
}

int Database::GetSets(class ScriptVector &names) const
{
    return GetSectionNames("pageset", names);
}

int Database::GetCollections(class ScriptVector &names) const
{
    return GetSectionNames("collection", names);
}

int Database::GetBinaries(class ScriptVector &names) const
{
    return GetSectionNames("binary", names);
}

int Database::GetAliasSections(ScriptVector &names) const
{
    return GetSectionNames("aliases", names);
}

int Database::GetSectionNames(const char *gn, class ScriptVector &names) const
{
    int cnt = inifile->GetSectionCount(gn);
    int i;
    names.Clear();
    for(i = 0; i < cnt; i++)
        names[i] = inifile->GetSectionName(gn, i);
    return cnt;
}

static int extract_chmod(IniFileParser *ini, const char *grp, const char *id)
{
    const char *tmp = ini->GetTextParameter(grp, id, "chmod", 0);
    if(!tmp)
        return 0;
    long mode;
    bool ok = ScriptVariable(tmp).GetLong(mode, 8);
    return ok ? mode : 0;
}

bool Database::BuildGenericFile(const ScriptVariable &id,
                          ScriptVariable &path, int &chmod_value,
                          ScriptVariable &cont) const
{
    const char *nm =
        inifile->GetTextParameter("genfile", id.c_str(), "path", 0);
    path = nm ? (*subst)(nm) : id;

    chmod_value = extract_chmod(inifile, "genfile", id.c_str());

    const char *cn =
        inifile->GetTextParameter("genfile", id.c_str(), "content", 0);
    if(!cn)
        return false;
    cont = (*subst)(cn);
    return true;
}

/* pages */

ScriptVariable Database::GetPageFilename(const ScriptVariable &name) const
{
    const char *path =
        inifile->GetTextParameter("page", name.c_str(), "path", 0);
    if(path && *path) {
        ScriptVariable pt(path);
        pt = (*subst)(pt);
        pt.Trim();
        if(pt == "" || pt == "." || pt == "-")
            return ScriptVariableInv();
        return GetFilePrefix() + pt;
    }
    return GetFilePrefix() + name;
}

int Database::GetPageChmod(const ScriptVariable &pg_id) const
{
    return extract_chmod(inifile, "page", pg_id.c_str());
}

ScriptVariable Database::BuildPage(const ScriptVariable &name) const
{
    const char *nc = name.c_str();
    const char *tmpl = inifile->GetTextParameter("page", nc, "template", 0);
    if(!tmpl) {
        const char *res = inifile->GetTextParameter("page", nc, "body", 0);
        if(!res)
            return ScriptVariableInv();
        return (*subst)(res);
    }
    return ExpandTemplate(tmpl, "page", nc);
}


static bool boolean_value_from_string(const char *str)
{
    return str && ScriptVariable(str).Trim().Tolower() == "yes";
}

static bool boolean_value_from_scrvar(ScriptVariable s)
{
    return s.IsValid() && s.Trim().Tolower() == "yes";
}


/* lists */

static list_source_type srctype_by_name(ScriptVariable s)
{
    s.Trim();
    s.Tolower();
    if(s == "ini")
        return listsrc_ini;
    if(s == "set")
        return listsrc_set;
    return listsrc_unknown;
}

static void reverse_script_vector(ScriptVector &v)
{
    int i, j;
    for(i = 0, j = v.Length()-1; i < j; i++, j--) {
        ScriptVariable t = v[i];
        v[i] = v[j];
        v[j] = t;
    }
}

bool Database::IsListEmbedded(const ScriptVariable &list_id) const
{
    const char *lsid = list_id.c_str();
    const char *tmp = inifile->GetTextParameter("list", lsid, "embedded", 0);
    return boolean_value_from_string(tmp);
}

bool Database::GetListData(const ScriptVariable &list_id, ListData &data) const
{
    data.id = list_id;

    data.embedded = IsListEmbedded(list_id);

    const char *lsid = list_id.c_str();

    const char *srcstr = inifile->GetTextParameter("list", lsid, "source", 0);
    if(srcstr) {
        ScriptWordVector v(srcstr);
        data.srctype = srctype_by_name(v[0]);
        if(data.srctype == listsrc_unknown)
            return false;
        data.srcname = v.Length() < 2 ? list_id : v[1];
        data.tag = v.Length() < 3 ? "" : v[2];
        if(data.srctype == listsrc_set &&
            (data.tag.IsInvalid() || data.tag == ""))
        {
            return false;
        }
    } else {
#if 0
        data.srctype = listsrc_ini;
        data.srcname = list_id;
#else
        return false;
#endif
    }

    const char *tmp;

    tmp = inifile->GetTextParameter("list", lsid, "reverse", 0);
    data.reverse = boolean_value_from_string(tmp);
    tmp = inifile->GetTextParameter("list", lsid, "reverse_source", 0);
    bool reverse_source = boolean_value_from_string(tmp);

    data.list_header =
        inifile->GetTextParameter("list", lsid, "list_header", "");
    data.list_footer =
        inifile->GetTextParameter("list", lsid, "list_footer", "");
    data.list_item_templ =
        inifile->GetTextParameter("list", lsid, "list_item_template", "");

    if(data.embedded) {
        data.items_per_listpage = 0;
        data.main_listpage_name.Invalidate();
        data.listpage_name_templ.Invalidate();
    } else {
        data.items_per_listpage =
            inifile->GetIntegerParameter("list",lsid, "items_per_listpage", 0);

        tmp = inifile->GetTextParameter("list", lsid, "main_listpage_name", 0);
        data.main_listpage_name = tmp ? ScriptVariable(tmp) : list_id;
        tmp = inifile->GetTextParameter("list",lsid, "listpage_name_templ", 0);
        data.listpage_name_templ =
            tmp ? ScriptVariable(tmp) : list_id + "_p%n%";
    }

    if(data.srctype == listsrc_ini) {
        ScriptVariable auxp =
            inifile->GetTextParameter("list", lsid, "aux_params", "");
        data.aux_params.Clear();
        data.aux_params = ScriptVector(auxp, " ,\t\r\n");
    }

    tmp = inifile->GetTextParameter("list", lsid, "pages", 0);
    data.pages = boolean_value_from_string(tmp);
    if(data.pages) {
        data.itempage_templ =
            inifile->GetTextParameter("list", lsid, "itempage_template", "");
        data.itempage_tail_templ =
            inifile->GetTextParameter("list",lsid,"itempage_tail_template","");
        data.comments_conf =
            inifile->GetTextParameter("list", lsid, "comments", "");
    }

    switch(data.srctype) {
    case listsrc_ini:
        GetSectionNames(data.srcname.c_str(), data.items);
        break;
    case listsrc_set:
        ScanSetTagItems(data.srcname, data.tag, data.items);
        break;
    default:
        return false;
    }

    if(reverse_source)
        reverse_script_vector(data.items);

    int last_only =
        inifile->GetIntegerParameter("list", lsid, "last_items_only", 0);
    if(last_only > 0 && data.items.Length() > last_only) {
        data.items.Remove(0, data.items.Length() - last_only);
    }

    return true;
}

void Database::GetListNavigationInfo(const ScriptVariable &name,
    int &items_per_page, ScriptVariable &set_id, ScriptVariable &tag) const
{
    const char *lsid = name.c_str();

    set_id = "";
    tag = "";

    const char *srcstr = inifile->GetTextParameter("list", lsid, "source", 0);
    if(srcstr) {
        ScriptWordVector v(srcstr);
        if(v[0] == "set") {
            set_id = v[1];
            tag = v[2];
        }
    }

    items_per_page =
        inifile->GetIntegerParameter("list", lsid, "items_per_listpage", 0);
}


bool Database::GetSetListIndexInfo(const ScriptVariable &set_id,
                                   const ScriptVariable &tag,
                                   const ScriptVariable &item_id,
                                   int &index,
                                   ScriptVariable &prev,
                                   ScriptVariable &next) const
{
    SetListIndex *sli =
        const_cast<Database*>(this)->ProvideSetListIndex(set_id, tag);
    if(!sli)
        return false;
    int i;
    int len = sli->items.Length();
    for(i = 0; i < len; i++)
        if(sli->items[i] == item_id) {
            index = i + 1;  // they are 1-based
            prev = i > 0 ? sli->items[i-1] : ScriptVariableInv();
            next = i < len-1 ? sli->items[i+1] : ScriptVariableInv();
            return true;
        }
    return false;
}

// XXX this function should perhaps disappear!
// instead, the database should provide a method to generate textual date
static void fill_date_from_unixtime(ListItemData &data)
{
    if(data.unixtime > 0) {
        time_t tt = data.unixtime;
        data.date = ScriptVariable(asctime(gmtime(&tt))) + " UTC";
            // XXX we need more flexibility here!
    } else {
        data.date =
            ScriptVariable("NODATE[") + data.item_id + "]";
    }
}

bool Database::GetListItemData(const ListData &lsdata, int idx,
                               ListItemData &data) const
{
    const ScriptVariable &ls_name = lsdata.srcname;
    const ScriptVariable &item_id = lsdata.items[idx];
    ScriptVariableInv inval;

    data.make_separate_directory = false;
        // for ``ini-sourced'' lists the opposit isn't supported

    data.item_id = item_id;
    data.index = idx+1;  // remember they are 1-based
    data.prev_id = idx > 0 ? lsdata.items[idx-1] : inval;
    data.next_id = idx+1 < lsdata.items.Length() ? lsdata.items[idx+1] : inval;

    const char *lsc = ls_name.c_str();
    const char *idc = item_id.c_str();

    const char *utime_str = inifile->GetTextParameter(lsc, idc, "unixtime", 0);
    if(!utime_str || !*utime_str) {
        data.unixtime = 0;
    } else {
        long long t;
        bool ok = ScriptVariable(utime_str).GetLongLong(t, 10);
        data.unixtime = ok ? t : -1;
    }
    const char *date_str = inifile->GetTextParameter(lsc, idc, "date", 0);
    if(date_str)
        data.date = (*subst)(date_str);
    else
        fill_date_from_unixtime(data);

    data.title = (*subst)(inifile->GetTextParameter(lsc, idc, "title", ""));
    data.descr = (*subst)(inifile->GetTextParameter(lsc, idc, "descr", ""));

    ScriptVariable tagsstr =
        (*subst)(inifile->GetTextParameter(lsc, idc, "tags", ""));
    data.tags.Clear();
    data.tags = ScriptVector(tagsstr, ",", " \t\r\n");

    data.comments = inifile->GetTextParameter(lsc, idc, "comments", "");

    int i;
    for(i = 0; i < lsdata.aux_params.Length(); i++) {
        const char *ap =
            inifile->GetTextParameter(lsc, idc,
                                      lsdata.aux_params[i].c_str(), 0);
        if(!ap)
            continue;
        data.aux_params.AddItem(lsdata.aux_params[i]);
        data.aux_params.AddItem((*subst)(ap));
    }

    data.text = (*subst)(inifile->GetTextParameter(lsc, idc, "text", ""));
    return true;   // XXXXXXX well... is this really okay?
}

ScriptVariable
Database::GetListPgpath(const ScriptVariable &ls_id, int num) const
{
    if(num < 1) {
        const char *tmp =
            inifile->GetTextParameter("list", ls_id.c_str(),
                                      "main_listpage_name", 0);
        if(tmp)
            return (*subst)(tmp);
    }
    ScriptVariable res =
        inifile->GetTextParameter("list", ls_id.c_str(),
                                  "listpage_name_templ", "");
    if(res == "") {
        res = ls_id + "_p" + ScriptNumber(num) + ".html";
    } else {
        ScriptMacroprocessor sub_sub(subst);
        add_index_macros(sub_sub, num);
        res = sub_sub(res);
    }
    return res;
}

ScriptVariable
Database::GetListFilename(const ScriptVariable &ls_id, int num) const
{
    return GetFilePrefix() + GetListPgpath(ls_id, num);
}

ScriptVariable
Database::GetListHref(const ScriptVariable &ls_id, int num) const
{
    return GetBaseUri() + GetListPgpath(ls_id, num);
}

ScriptVariable Database::GetListItemFilename(const ScriptVariable &ls_id,
                                             const ScriptVariable &pg_id,
                                             int pg_idx) const
{
    ScriptVariable tmp =
        inifile->GetTextParameter("list", ls_id.c_str(), "itempage_name", "");
    if(tmp == "")
        tmp = ls_id + "_" + pg_id + "%_idx%.html";
    ScriptMacroprocessor sub_sub(subst);
    add_index_macros(sub_sub, pg_idx);
    return GetFilePrefix() + sub_sub(tmp);
}

ScriptVariable Database::GetListItemCommentmapFname(
              const ScriptVariable &ls_id, const ScriptVariable &pg_id) const
{
    const char *tmp =
        inifile->GetTextParameter("list", ls_id.c_str(), "commentmap", 0);
    if(!tmp)
        return ScriptVariableInv();
    return GetFilePrefix() + (*subst)(tmp);
}

ScriptVariable Database::GetListItemHref(const ScriptVariable &ls_id,
                                         const ScriptVariable &pg_id,
                                         int pg_idx) const
{
    ScriptVariable tmp = inifile->GetTextParameter("list", ls_id.c_str(),
                                                   "itempage_name", "");
    ScriptMacroprocessor sub_sub(subst);
    add_index_macros(sub_sub, pg_idx);
    return GetBaseUri() + sub_sub(tmp);
}


// IMPORTANT: there must be a single object for substitutions inside the
// database, being created within the constructor
//  It must be able to "know" and "forget" the ListData/ListItem structs

ScriptVariable Database::BuildListHead(const ListData &lsd) const
{
    //SetListData(&lsd);
    ScriptVariable res = (*subst)(lsd.list_header);
    //ForgetListCmtData();
    return res;
}

ScriptVariable Database::BuildListTail(const ListData &lsd) const
{
    //SetListData(&lsd);
    ScriptVariable res = (*subst)(lsd.list_footer);
    //ForgetListCmtData();
    return res;
}

ScriptVariable Database::BuildListItem(const ListData &lsd,
                                       const ListItemData &itd) const
{
    return BuildListItemPart(lsd, itd, lsd.list_item_templ);
}

ScriptVariable Database::BuildListItemPart(const ListData &lsd,
                                           const ListItemData &itd,
                                           const ScriptVariable &templ) const
{
    //SetListData(&lsd, &itd);
    ScriptVariable res = (*subst)(templ);
    //ForgetListCmtData();
    return res;
}

void Database::BuildListItemPage(const ListData &lsd, const ListItemData &itd,
                       ScriptVariable &main, ScriptVariable &tail,
                       ForumGenerator **fgp) const
{
    main = BuildListItemPart(lsd, itd, lsd.itempage_templ);
    tail = BuildListItemPart(lsd, itd, lsd.itempage_tail_templ);
    *fgp = GetComments(lsd.comments_conf);
}


#if 0
void Database::FillArrayDataForList(const ListData &lsd, ArrayData &ad) const
{
    int itemcnt = lsd.items.Length();
    int perpage = lsd.items_per_listpage;

    ad.item_count = itemcnt;
    ad.items_per_page = perpage;
    ad.page_count = perpage <= 0 ? 1 : (itemcnt ? (itemcnt-1)/perpage+1 : 0);

    ad.reverse = lsd.reverse;

    ad.href_array.Clear();

    ScriptMacroprocessor sub_sub(subst);
    ScriptVariable v_idx;   // NB: no zero index is used here!
    sub_sub.AddMacro(new ScriptMacroScrVar("idx", &v_idx));
    sub_sub.AddMacro(new ScriptMacroScrVar("idx0", &v_idx));
    sub_sub.AddMacro(new ScriptMacroScrVar("_idx", &v_idx));

    int i;
    for(i = 1; i <= ad.page_count; i++) {
        v_idx = ScriptNumber(i);
        ad.href_array[i] = GetBaseUri() + sub_sub(lsd.listpage_name_templ);
    }

    if(ad.reverse) {
        ad.href_first = ScriptVariableInv();
        ad.href_last = GetBaseUri() + lsd.main_listpage_name;
    } else {
        ad.href_first = GetBaseUri() + lsd.main_listpage_name;
        ad.href_last = ScriptVariableInv();
    }

    ad.current_page   = -1;
}

void Database::FillArrayDataForListItem(const ListData &lsd,
                                        const ListItemData &itd,
                                        ForumGenerator *fgp,
                                        ArrayData &ad) const
{
    int pgcnt;
    bool reverse;

    ad.href_array.Clear();


    if(!fgp || !fgp->GetArrayInfo(pgcnt, reverse) || pgcnt < 2) {
        ad.item_count = 0;
        ad.items_per_page = 0;
        ad.page_count = 0;
        ad.current_page = -1;
        return;
    }

    ad.item_count = pgcnt;    // XXXX do we really need these two fields?!
    ad.items_per_page = 1;
    ad.page_count = pgcnt;
    ad.reverse = reverse;

    int i;
    for(i = 1; i <= pgcnt; i++)
        ad.href_array[i] = GetListItemHref(lsd.id, itd.item_id, i);

    ad.href_first = GetListItemHref(lsd.id, itd.item_id, 0);
    ad.href_last = ScriptVariableInv();

    ad.current_page   = -1;
}
#endif

#if 0
ScriptVariable Database::BuildListSegment(const ListData &lsd,
                                    const ScriptVariable &tag,
                                    const ScriptVariable &templ) const
{
    ScriptVariable res("");
    int i;
    for(i = 0; i < lsd.items.Length(); i++) {
        ListItemData itd;
        if(!GetListItemData(lsd, i, itd))
            continue;
        if(itd.HasTag(tag))
            res += BuildListItem(lsd, itd, templ);
    }
    return res;
}
#endif


static int method_by_name(const char *name)
{
    ScriptVariable s(name);
    s.Trim();
    s.Tolower();

    if(s == "copy")
        return fpm_copy;
    if(s == "link")
        return fpm_link;
    if(s == "symlink")
        return fpm_symlink;
    return fpm_none;
}

static int symlinks_mode_by_name(const char *name)
{
    ScriptVariable s(name);
    s.Trim();
    s.Tolower();

    if(s == "follow")
        return fpm_slnk_follow;
    if(s == "preserve")
        return fpm_slnk_preserve;
#if 0
    // XXX perhaps this should differ from the unknown case?
    if(s == "" || s == "ignore")
        return 0;
#endif
    return 0;
}

static int
extract_publish_method(IniFileParser *ini, const char *grp, const char *id)
{
    const char *tmp;
    int res;

    tmp = ini->GetTextParameter(grp, id, "publish_method", "none");
    res = method_by_name(tmp);

    tmp = ini->GetTextParameter(grp, id, "publish_recursive", 0);
    if(boolean_value_from_string(tmp))
        res |= fpm_recursive;

    tmp = ini->GetTextParameter(grp, id, "publish_hidden", 0);
    if(boolean_value_from_string(tmp))
        res |= fpm_includehidden;

    tmp = ini->GetTextParameter(grp, id, "publish_symlinks", "");
    res |= symlinks_mode_by_name(tmp);

    long mode = extract_chmod(ini, grp, id);
    if(mode)
        res |= (mode & fpm_mode_mask);

    return res;
}

ScriptVariable Database::GetSetSourceDir(const ScriptVariable &set_id) const
{
    const char *tmp =
        inifile->GetTextParameter("pageset", set_id.c_str(), "sourcedir", 0);
#if 0
    if(!tmp)
        return GetSourcePrefix() + set_id;
    if(*tmp == '/')
        return tmp;
    return GetSourcePrefix() + tmp;
#endif
    if(!tmp)
        return set_id;
    return tmp;
}

static int extract_make_subdirs_field(const char *s)
{
    if(!s)
        return pageset_subdir_bysource;
    ScriptVariable sv(s);
    sv.Trim();
    sv.Tolower();
    if(sv == "always")
        return pageset_subdir_always;
    if(sv == "never")
        return pageset_subdir_never;
    return pageset_subdir_bysource;
}

bool Database::
GetSetData(const ScriptVariable &set_id, PageSetData &data) const
{
    const char *tmp;
    ScriptVariable str;

    data.id = set_id;
    data.source_dir = GetSetSourceDir(set_id);

    const char *idc = set_id.c_str();

#if 0    // from now on, these fields may be modified with the pg ``type''
    data.page_templ =
        inifile->GetTextParameter("pageset", idc, "page_template", "");
    data.page_tail_templ =
        inifile->GetTextParameter("pageset", idc, "page_tail_template", "");
#endif

#if 0
    tmp = inifile->GetTextParameter("pageset", idc, "auxfiles", 0);
    data.aux_files.Clear();
    if(tmp)
        data.aux_files = ScriptWordVector(tmp);
#endif

    data.publish_method = extract_publish_method(inifile, "pageset", idc);

    tmp = inifile->GetTextParameter("pageset", idc, "make_subdirs", 0);
    data.make_subdirs = extract_make_subdirs_field(tmp);

    tmp = inifile->GetTextParameter("pageset", idc, "setdirname", 0);
    data.setdirname = tmp ? ScriptVariable(tmp) : set_id;

    tmp = inifile->GetTextParameter("pageset", idc, "pagedirname", 0);
    data.pagedirname = tmp ? ScriptVariable(tmp) : "%[li:id]";

    tmp = inifile->GetTextParameter("pageset", idc, "indexfilename", 0);
    data.indexfilename = tmp ? ScriptVariable(tmp) : "index.html";

    tmp = inifile->GetTextParameter("pageset", idc, "compagename", 0);
    data.compagename = tmp ? ScriptVariable(tmp) : "c%idx%.html";

    tmp = inifile->GetTextParameter("pageset", idc, "pagefilename", 0);
    data.pagefilename =
        tmp ? ScriptVariable(tmp) : "%[li:id]%[_idx].html";

    data.comments_conf =
        inifile->GetTextParameter("pageset", idc, "comments", "");

    tmp = inifile->GetTextParameter("pageset", idc, "commentmap", 0);
    data.cmap_filename = tmp ? ScriptVariable(tmp) : ScriptVariableInv();

    tmp = inifile->GetModifiedTextParameter("pageset", idc, "commentmap",
                                            "nodir", 0);
    data.cmap_filename_nodir =
        tmp ? ScriptVariable(tmp) : ScriptVariableInv();

    data.page_ids.Clear();

    return true;
}

void Database::ScanSetDirectory(PageSetData &data) const
{
    ReadDir dir(data.source_dir.c_str());
    const char *nm;
    while((nm = dir.Next())) {
        if(*nm == '.' || *nm == '_')
            continue;
        FileStat st((data.source_dir + "/" + nm).c_str());
        if(!st.Exists() || (!st.IsDir() && !st.IsRegularFile()))
            continue;
        // X-X-X implement sorting here! X-X-X really?...
        //     perhaps no, because it should be done upon dates not ids
        //  *NO* actually, all lists must already be built inside the
        //  database, it is senseless to sort every time instead of
        //  maintaining the lists in the sorted state
        data.page_ids.AddItem(nm);
    }
}

bool Database::ScanSetTagItems(const ScriptVariable &set_id,
                         const ScriptVariable &tag, ScriptVector &items) const
{
    SetListIndex *tmp =
        const_cast<Database*>(this)->ProvideSetListIndex(set_id, tag);
    if(!tmp)
        return false;
    items = tmp->items;
    return true;

#if 0
    ScriptVariable sd = GetSetSourceDir(set_id);
    ReadStream f;
    if(!f.FOpen((sd + "/" + tag).c_str()))
        return false;
    items.Clear();
    ScriptVector line;
    while(f.ReadLine(line, 2, ":", " \t\r\n")) {
        if(line.Length() < 2)
            continue;
        long idx;
        if(!line[0].GetLong(idx, 10))
            continue;
        // REMEMBER, indices are from 1, not 0, but the items indexed from 0
        if(idx < 1 || idx > THALASSA_LIST_INDEX_LIMIT)
            continue;
        items[idx-1] = line[1];
    }
    return true;
#endif
}

FilterChainSet *Database::MakeFormatFilter(const HeadedTextMessage &msg) const
{
    ProvideFilterChainMaker();
    return filtmaker->MakeChainSet(msg.GetHeaders());
}

bool Database::GetSetItemData(const PageSetData &setd, int idx,
                              ListItemData *itd) const
{
    if(idx >= setd.page_ids.Length())
        return false;

    return GetSetItemDataById(setd.id, setd.page_ids[idx], itd);
}

#if 0
static void read_set_item_navigation_hints(const char *path,
                                           ListItemData *itd)
{
    ReadStream nf;
    if(nf.FOpen(path)) {
        ScriptVariable wholefile;
        nf.ReadUntilEof(wholefile);
        nf.FClose();
        ScriptVector strs(wholefile, "\n");
        if(strs.Length() > 0) {
            itd->nav_hints_count = strs.Length();
            itd->nav_hints = new NavigationHints[strs.Length()];
            int i;
            for(i = 0; i < strs.Length(); i++) {
                ScriptTokenVector nav(strs[i], ":", " \t\r\n");
                itd->nav_hints[i].key = nav[0];
                long ix;
                itd->nav_hints[i].index =
                    (nav[1].GetLong(ix, 10) && ix >= 0) ? ix : -1;
                itd->nav_hints[i].prev = nav[2];
                itd->nav_hints[i].cur = nav[3];
                itd->nav_hints[i].next = nav[4];
            }
            itd->index = itd->nav_hints[0].index;
            itd->prev_id = itd->nav_hints[0].prev;
            itd->next_id = itd->nav_hints[0].next;
        }
    }
}
#endif

static void scan_set_item_dir_for_files(ScriptVariable path, ScriptVector &v)
{
    v.Clear();
    ReadDir dir(path.c_str());
    const char *nm;
    while((nm = dir.Next())) {
        if(*nm == '.' || *nm == '_')
            continue;
        ScriptVariable nms(nm);
        if(nms == PAGESET_MAIN_FILE_NAME)
            continue;
        FileStat st((path + "/" + nms).c_str());
        if(!st.Exists() || !st.IsRegularFile())
            continue;
        v.AddItem(nm);
    }
}

bool Database::
GetSetItemSource(const ScriptVariable &set_id, const ScriptVariable &page_id,
           ScriptVariable &srcd, ScriptVariable &fname, bool &dedic_dir) const
{
    static ScriptVariable saved_set_id(0), set_src_path(0);

    if(saved_set_id != set_id) {
        saved_set_id = set_id;
        set_src_path = GetSetSourceDir(set_id);
    }

    srcd = set_src_path + "/" + page_id;

    FileStat st(srcd.c_str());
    if(!st.Exists())
        return false;

    if(st.IsRegularFile()) {
        fname = srcd;
        dedic_dir = false;
    } else {
        fname = srcd + "/" PAGESET_MAIN_FILE_NAME;
        dedic_dir = true;
    }

    return true;
}

bool Database::GetSetItemDataById(const ScriptVariable &set_id,
                                  const ScriptVariable &page_id,
                                  ListItemData *itd) const
{
    itd->item_id = page_id;

    ScriptVariable srcd, fname;
    bool it_is_dir;
    bool ok = GetSetItemSource(set_id, page_id, srcd, fname, it_is_dir);
    if(!ok) {
        itd->title = fname + ": file doesn't exist";
        return false;
    }

    itd->make_separate_directory = it_is_dir;

    FILE *s = fopen(fname.c_str(), "r");
    if(!s) {
        itd->title = fname + ": couldn't open file";
        return false;
    }
    int c;
    HeadedTextMessage parser(false);
    while((c = fgetc(s)) != EOF) {
        if(!parser.FeedChar(c))   // ok, int
            break;
    }
    fclose(s);

    long teaser_len = -1;

    FilterChainSet *filt = MakeFormatFilter(parser);
    int i;
    const ScriptVector& hdr = parser.GetHeaders();
    for(i = 0; i < hdr.Length()-1; i+=2) {
        if(hdr[i] == "id" || hdr[i] == "encoding" || hdr[i] == "format") {
            // no storing
            continue;
        }
        if(hdr[i] == "unixtime") {
            bool ok = hdr[i+1].GetLongLong(itd->unixtime, 10);
            if(!ok)
                itd->unixtime = -1;
            continue;
        }
        if(hdr[i] == "type") {
            itd->pgtype = hdr[i+1];  // no conversion here, it's a enum id
            continue;
        }
        if(hdr[i] == "flags") {
            itd->flags.Clear();
            itd->flags = ScriptVector(hdr[i+1], ",", " \t\r\n");
            continue;
        }
        if(hdr[i] == "comments") {
            itd->comments = hdr[i+1];  // no conversion here, it's a enum id
            continue;
        }
        if(hdr[i] == "teaser_len") {
            bool ok = hdr[i+1].GetLong(teaser_len, 10);
            if(!ok)
                teaser_len = -1;
            continue;
        }
        // don't forget there's also filt->ConvertData
        // for the data actually generated by the system
        // NOT entered by users
        if(hdr[i] == "descr") {
            itd->descr = filt->ConvertContent(hdr[i+1]);
            continue;
        }
        // the rest of fields are converted as Userdata
        ScriptVariable val = filt->ConvertUserdata(hdr[i+1]);
        if(hdr[i] == "date") {
            itd->date = val;
            continue;
        }
        if(hdr[i] == "title") {
            itd->title = val;
            continue;
        }
        if(hdr[i] == "tags") {
            itd->tags.Clear();
            itd->tags = ScriptVector(val, ",", " \t\r\n");
            continue;
        }
        // okay, no fields for this parameter in the structure
        // just store it in the aux_params vector
        itd->aux_params.AddItem(hdr[i]);
        itd->aux_params.AddItem(val);
    }
    if(itd->date == "")
        fill_date_from_unixtime(*itd);

    itd->text = filt->ConvertContent(parser.GetBody());

    if(teaser_len > 0 && (itd->descr.IsInvalid() || itd->descr == "")) {
        ScriptVariable b = parser.GetBody();
        //parser.GetBody() = ""; // optimization
        itd->descr = filt->ConvertContent(b.Range(0, teaser_len).Get());
    }

    delete filt;

    if(it_is_dir)
        scan_set_item_dir_for_files(srcd, itd->files);
    else
        itd->files.Clear();

    return true;
}

void Database::GetSetItemFilenames(const PageSetData &setd, bool separ_dir,
                           ScriptVariable &dir, ScriptVariable &idxfl,
                           ScriptVariable &href, ScriptVariable &cmap) const
{
    if(separ_dir) {
        href = (*subst)(setd.setdirname + "/" + setd.pagedirname);
        dir = GetFilePrefix() + href;
        idxfl = dir + "/" + setd.indexfilename;
    } else {
        ScriptVariable d = (*subst)(setd.setdirname);
        ScriptMacroprocessor sub_sub(subst);
        add_index_macros(sub_sub, 0);   // XXX really 0, not -1 ?
        ScriptVariable p = sub_sub(setd.pagefilename);
        dir = GetFilePrefix() + d;
        idxfl = dir + "/" + p;
        href = d + "/" + p;
    }
    if(href[0] != '/')
        href.Range(0,0).Replace("/");
    ScriptVariable m =
        separ_dir ? setd.cmap_filename : setd.cmap_filename_nodir;
    cmap = m.IsValid() ? dir + "/" + (*subst)(m) : ScriptVariableInv();
}

void Database::
GetSetSubitemFilename(const PageSetData &setd, bool separ_dir, int subidx,
                     ScriptVariable &fname, ScriptVariable &href) const
{
    ScriptMacroprocessor sub_sub(subst);
    add_index_macros(sub_sub, subidx);
    ScriptVariable dir;
    ScriptVariable locpart;
    if(separ_dir) {
        dir = GetFilePrefix() +
            sub_sub(setd.setdirname + "/" + setd.pagedirname);
        locpart = sub_sub(setd.compagename);
    } else {
        dir = GetFilePrefix() + sub_sub(setd.setdirname);
        locpart = sub_sub(setd.pagefilename);
    }
    fname = dir + "/" + locpart;
    href = locpart;
}

void Database::
BuildSetItemPage(const PageSetData &psd, const ListItemData &itd,
                 ScriptVariable &main, ScriptVariable &tail,
                 ForumGenerator **fgp) const
{
    const char *idc = psd.id.c_str();
    const char *ts = itd.pgtype.c_str();
    ScriptVariable page_templ =
        inifile->GetModifiedTextParameter("pageset", idc,
                                          "page_template", ts, "");
    ScriptVariable page_tail_templ =
        inifile->GetModifiedTextParameter("pageset", idc,
                                          "page_tail_template", ts, "");
    //SetListData(0, &itd);
    main = (*subst)(page_templ);
    tail = (*subst)(page_tail_templ);
    if(itd.comments == "enabled" || itd.comments == "readonly")
        *fgp = GetComments(psd.comments_conf);
    else
        *fgp = new EmptyForumGenerator(this);
    //ForgetListCmtData();
}

bool Database::GetCollectionData(const ScriptVariable &id,
                           ScriptVariable &srcdir, ScriptVariable &dstdir,
                           int &method) const
{
    const char *idc = id.c_str();

    srcdir = (*subst)(
        inifile->GetTextParameter("collection", idc, "sourcedir", idc));
    dstdir = GetFilePrefix() + (*subst)(
        inifile->GetTextParameter("collection", idc, "destdir", idc));

    method = extract_publish_method(inifile, "collection", idc);

    return (method & fpm_method_mask) != fpm_none;
}

bool Database::GetBinaryData(const ScriptVariable &id,
                           ScriptVariable &src, ScriptVariable &dst,
                           int &method) const
{
    const char *idc = id.c_str();

    src = (*subst)(inifile->GetTextParameter("binary", idc, "source", idc));
    dst = GetFilePrefix() +
        (*subst)(inifile->GetTextParameter("binary", idc, "dest", idc));

    method = extract_publish_method(inifile, "binary", idc);

    return (method & fpm_method_mask) != fpm_none;
}


void Database::GetAliases(const ScriptVariable &id, ScriptVector &result) const
{
    result.Clear();
    const char *tmp =
        inifile->GetTextParameter("aliases", id.c_str(), "aliases", 0);
    if(!tmp)
        return;
    ScriptVector lines((*subst)(tmp), "\n", " \t\r");
    int n = lines.Length();
    int i;
    for(i = 0; i < n; i++) {
        ScriptVector toks(lines[i], ":", " \t\r");
        if(toks.Length() < 2)   // XXX we simply ignore ill-formed lines
            continue;           //     this might be a bad idea
        result.AddItem(toks[0]);
        result.AddItem(toks[1]);
    }
}

void Database::GetAliasDirs(const ScriptVariable &id, ScriptVector &res) const
{
    const char *s =
        inifile->GetTextParameter("aliases", id.c_str(), "force_dirs", "");
    res = ScriptVector(s, ",", " \t\r\n");
}

ScriptVariable Database::GetAliasDirFilename(const ScriptVariable &id) const
{
    return inifile->GetTextParameter("aliases", id.c_str(),
                                     "dir_file_name", ".htaccess");
}

ScriptVariable Database::BuildAliasDirFile(const ScriptVariable &id,
                                const ScriptVariable &target_uri) const
{
    ScriptVariable templ = inifile->GetTextParameter("aliases", id.c_str(),
                                               "dir_file_template", "");
    ScriptMacroprocessor sub_sub(subst);
    sub_sub.AddMacro(new ScriptMacroConst("target", target_uri));
    return sub_sub.Process(templ);
}

#if 0
bool Database::GetTextMaybeFile(const char *g, const char *gs, const char *p,
                                ScriptVariable &res) const
{
    res = inifile->GetTextParameter(g, gs, p, "");
    if(res.Length() >= too_long_for_filename)
        return true;
    res.Trim(" \r\t");  // don't strip end-of-line!
    if(res[0] != '>')
        return true;

    res.Range(0,1).Erase();
    res.Trim();
#if 0
    res = GetSourcePrefix() + res;
#endif

    ReadStream s;
    if(!s.FOpen(res.c_str())) {
        res += ": couldn't open file";
        return false;
    }
    s.ReadUntilEof(res);
    s.FClose();

    return true;
}
#endif

ScriptVariable
Database::ExpandTemplate(const char *templ, const char *g, const char *s) const
{
    ScriptVariable body =
        inifile->GetTextParameter("template", templ, "body", "");
    ScriptVariable par =
        inifile->GetTextParameter("template", templ, "params", "");
    ScriptWordVector paramsv(par);

    ScriptMacroprocessor sub_sub(subst);
    int i;
    for(i = 0; i < paramsv.Length(); i++) {
        ScriptVariable val =
            inifile->GetTextParameter(g, s, paramsv[i].c_str(), "");
        sub_sub.AddMacro(new ScriptMacroConst(paramsv[i], (*subst)(val)));
    }
    //ScriptSubstitutionDictionary dv(dict, false);
    //return subst->Substitute(body, &dv);
    return sub_sub.Process(body);
}

void Database::ProvideFilterChainMaker() const
{
    if(filtmaker)
        return;
    const char *enc = inifile->GetTextParameter("format", 0, "encoding", 0);
    const char *tags = inifile->GetTextParameter("format", 0, "tags", "");
    const char *attrs =
        inifile->GetTextParameter("format", 0, "tag_attributes", 0);
    if(!attrs)
        attrs = "a=href img=src img=alt";

    ScriptVariable enc_sv = enc ? (*subst)(enc) : ScriptVariableInv();
    ScriptVariable tags_sv = (*subst)(tags);
    ScriptVariable attrs_sv = (*subst)(attrs);

    const_cast<Database*>(this)->filtmaker =
        new FilterChainMaker(enc ? enc_sv.c_str() : 0,
                             tags_sv.c_str(), attrs_sv.c_str());
}

SetListIndex *Database::
ProvideSetListIndex(const ScriptVariable &set_id, const ScriptVariable &tag)
{
    SetListIndex *tmp;

    for(tmp = first_set_list; tmp; tmp = tmp->next)
        if(tmp->set_id == set_id && tmp->tag == tag)
            return tmp;

    ScriptVariable sd = GetSetSourceDir(set_id);
    if(sd.IsInvalid() || sd == "")
        return 0;

    ReadStream f;
    if(!f.FOpen((sd + "/_" + tag).c_str()))
        return 0;

    tmp = new SetListIndex;
    tmp->next = first_set_list;
    tmp->set_id = set_id;
    tmp->tag = tag;
    first_set_list = tmp;
    tmp->items.Clear();

    ScriptVariable line;
    while(f.ReadLine(line)) {
        line.Trim();
        if(line == "")
            continue;
        tmp->items.AddItem(line);
    }
    return tmp;
}

bool Database::
GetIndexBarStyle(const ScriptVariable &name, IndexBarStyle &s) const
{
    const char *sn = name.c_str();
    if(!sn || !*sn)
        return false;

    s.begin = inifile->GetTextParameter("indexbar", sn, "begin", "");
    s.end = inifile->GetTextParameter("indexbar", sn, "end", "");
    s.textbreak = inifile->GetTextParameter("indexbar", sn, "break", "");
    s.link = inifile->GetTextParameter("indexbar", sn, "link", "");
    s.greylink = inifile->GetTextParameter("indexbar", sn, "greylink", "");
    s.curpos = inifile->GetTextParameter("indexbar", sn, "curpos", "");
    s.textprev = inifile->GetTextParameter("indexbar", sn, "textprev", "");
    s.textnext = inifile->GetTextParameter("indexbar", sn, "textnext", "");
    s.textfirst = inifile->GetTextParameter("indexbar", sn, "textfirst", "");
    s.textlast = inifile->GetTextParameter("indexbar", sn, "textlast", "");
    s.tailsize = inifile->GetIntegerParameter("indexbar", sn, "tailsize", 2);

    return s.link.IsValid() && s.link != "";
}

#if 0
ScriptVariable
Database::BuildArrayIndex(const IndexBarStyle &style,
                          const ScriptVariable &anchor) const
{
    if(!array_data)
        return ScriptVariableInv();

    IndexInformation ixi;


    ixi.anchor = anchor;

    return build_array_index(&ixi, array_data);
}
#endif

ArrayData* Database::InstallArrayData(ArrayData *p)
{
    ArrayData *tmp = array_data;
    array_data = p;
    return tmp;
}

void Database::SetMacroData(const ListData *ld, const ListItemData *lid) const
{
    subst->SetData(ld, lid);
    const_cast<Database*>(this)->current_list_data = ld;
    const_cast<Database*>(this)->current_list_item_data = lid;
}

void Database::ForgetMacroData() const
{
    subst->ForgetData();
    const_cast<Database*>(this)->current_list_data = 0;
    const_cast<Database*>(this)->current_list_item_data = 0;
}

void Database::SaveMacroRealm(Database::MacroRealm &buf) const
{
    buf.ld = current_list_data;
    buf.lid = current_list_item_data;
}

void Database::RestoreMacroRealm(const Database::MacroRealm &buf) const
{
    SetMacroData(buf.ld, buf.lid);
}


static inline bool is_whitespace(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

ScriptVariable
Database::BuildMenu(const ScriptVariable &id, const ScriptVariable &cur) const
{
    const char *idc = id.c_str();
    if(!idc || !*idc)
        return ScriptVariableInv();

    const char *items = inifile->GetTextParameter("menu", idc, "items", 0);
    if(!items)
        return ScriptVariableInv();
    while(*items && is_whitespace(*items))
        items++;
    if(!*items)
        return ScriptVariableInv();

    char delim[2] = { 0, 0 };
    delim[0] = *items;
    items++;
    ScriptVector items_v((*subst)(items), delim, " \t\r\n");
    int len = items_v.Length();
    len = (len + 3) / 4;   // count of items, full or partial

    ScriptVariable link_templ =
        inifile->GetTextParameter("menu", idc, "link", "");
    ScriptVariable curpos_templ =
        inifile->GetTextParameter("menu", idc, "curpos", "");
    ScriptVariable tag_templ =
        inifile->GetTextParameter("menu", idc, "tag", "");
    ScriptVariable break_templ =
        inifile->GetTextParameter("menu", idc, "break", "");

    ScriptVariable result =
        (*subst)(inifile->GetTextParameter("menu", idc, "begin", ""));

    int i;
    for(i = 0; i < len; i++) {
        ScriptVariable uri = items_v[i*4+1];
        ScriptVariable *t = 0;
        if(uri.IsInvalid())
            uri = "";
        uri.Trim();
        if(uri == "") {
            ScriptVariable lab = items_v[i*4];
            lab.Trim();
            t = lab == "" ? &break_templ : &tag_templ;
        } else
        if(cur.IsValid() && cur != "" && cur == items_v[i*4 + 3]) {
            t = &curpos_templ;
        } else {
            t = &link_templ;
        }
        result += subst->Process(*t, items_v, i * 4, 4);
    }

    result += (*subst)(inifile->GetTextParameter("menu", idc, "end", ""));

    return result;
}

///////////////////////////////////////////////////////////////
// comments infrastructure

ScriptVariable Database::GetCommentsPath(ScriptVariable conf) const
{
    ScriptVector cflines((*subst)(conf), "\n", " \t\r");
    ScriptWordVector cfw(cflines[0]);
    if(cfw.Length() < 2)
        return ScriptVariableInv();
    return cfw[1];
}

ForumGenerator *Database::GetComments(ScriptVariable conf) const
{
    ScriptVector cflines((*subst)(conf), "\n", " \t\r");
    ScriptWordVector cfw(cflines[0]);
    if(cfw.Length() < 2) {
        return new ErrorForumGenerator(this,
            ScriptVariable("comments: style and path must be specified"));
    }
    ScriptVariable style = cfw[0];
    ScriptVariable path = cfw[1];

    ScriptVector auxparamdict;
    int i;
    for(i = 1 /* NB: 1, sic! */; i < cflines.Length(); i++) {
        ScriptVariable::Substring w1;
        cflines[i].Whole().FetchWord(w1);
        ScriptVariable name = w1.Get();
        if(name.IsInvalid())
            continue;
        name.Trim();
        if(name == "")
            continue;
        ScriptVariable value = w1.After().Get();
        if(value.IsInvalid())
            continue;
        value.Trim();
        auxparamdict.AddItem(name);
        auxparamdict.AddItem(value);
    }

    ForumData *fd = new ForumData;

    const char *st = style.c_str();
    static const char cmtst[] = "commentstyle";

    fd->type = (*subst)(inifile->GetTextParameter(cmtst, st, "type", ""));
    fd->top = inifile->GetTextParameter(cmtst, st, "top_template", "");
    fd->bottom = inifile->GetTextParameter(cmtst, st, "bottom_template", "");
#if 0
    fd->indent = inifile->GetTextParameter(cmtst, st, "indent_template", "");
    fd->unindent =
        inifile->GetTextParameter(cmtst, st, "unindent_template", "");
#endif
    fd->comment_templ =
        inifile->GetTextParameter(cmtst, st, "comment_template", "");
    fd->tail =
        inifile->GetTextParameter(cmtst, st, "comment_tail_template", "");
    fd->no_comments_templ =
        inifile->GetTextParameter(cmtst, st, "no_comments", "");

    ScriptVariable s;

    s = (*subst)(inifile->GetTextParameter(cmtst, st, "reverse", "no"));
    fd->reverse = boolean_value_from_scrvar(s);

    s = (*subst)(inifile->GetTextParameter(cmtst,st,"hidden_hold_place","no"));
    fd->hidden_hold_place = boolean_value_from_scrvar(s);

    s = (*subst)(inifile->GetTextParameter(cmtst, st, "perpage", "0"));
    long ppg;
    bool ok = s.GetLong(ppg, 10);
    fd->per_page = ok ? ppg : 0;


#if 0
    ScriptVariable custom_params =
        inifile->GetTextParameter(cmtst, st, "custom_params", "");
    ScriptVector cpvec(custom_params, " \t\n,");
#else
    ScriptVector cpvec;
#endif
    if(fd->type == "tree") {
        cpvec.AddItem("indent_template");
        cpvec.AddItem("unindent_template");
    } else
    if(fd->type == "thread") {
        // to be determined...
    }
    for(i = 0; i < cpvec.Length(); i++) {
        const char *s =
            inifile->GetTextParameter(cmtst, st, cpvec[i].c_str(), 0);
        if(!s || !*s)
            continue;
        fd->custom_params.AddItem(cpvec[i]);
        fd->custom_params.AddItem(s);
    }

    CommentDir cmtdir(path, auxparamdict);
    CommentTree *tree = cmtdir.GetTree();
    if(!tree) {
        delete fd;
        return new ErrorForumGenerator(this,
            ScriptVariable("error scanning the comment tree"));
    }
    cmtdir.ReleaseTree();

    if(fd->type == "list")
        return new PlainForumGenerator(this, fd, tree);
    if(fd->type == "tree") {
        if(fd->per_page == 0)
            return new SingleTreeForumGenerator(this, fd, tree);
        return new ForestForumGenerator(this, fd, tree);
    }
    if(fd->type == "thread")
        return new ThreadPageForumGenerator(this, fd, tree);

        // still here?!
    ScriptVariable errmsg("unsupported comment style ");
    errmsg += fd->type;
    delete fd;
    delete tree;
    return new ErrorForumGenerator(this, errmsg);
}

#if 0
ScriptVariable Database::BuildCommentSection(ForumGenerator *fg,
           const ScriptVariable &href, ScriptVariable &cmtmap) const
{
    if(!fg)
        return ScriptVariableInv();
    ScriptVariable res;
    bool ok;
    ok = fg->NextPage(href, res);
    if(!ok) {
        cmtmap = fg->MakeCommentMap();
        //CancelComments(fg);
        return ScriptVariableInv();
    }
    cmtmap = ScriptVariableInv();
    return res;
}
#endif

ScriptVariable Database::
BuildCommentPart(const CommentData &cdt, const ScriptVariable &templ) const
{
    subst->SetCmtData(&cdt);
    ScriptVariable res = (*subst)(templ);
    subst->SetCmtData(0);
    return res;
}

#if 0
void Database::CancelComments(Database::comment_iter *ci) const
{
    delete *(ForumGenerator**)ci;
    *ci = 0;
}
#endif


/////////////////////////////////////////
// blocks infrastructure

static void
add_block(BlockGroupData **first_group, const char *grp_id,
          const char *blk_id, int weight, const char *tag,
          const char *title, const char *body)
{
    BlockGroupData **grpp = first_group;
    while(*grpp && (*grpp)->id != grp_id)
        grpp = &((*grpp)->next_grp);
    if(!*grpp)
        *grpp = new BlockGroupData(grp_id, "", "", "%blk:title% %blk:body%");
    // now it is guaranteed that *grpp points to the desired group object
    BlockData **blkp = &((*grpp)->first);
    while(*blkp && (*blkp)->weight < weight)
        blkp = &((*blkp)->next);
    BlockData *bd = new BlockData(blk_id, weight, tag, title, body);
    bd->next = *blkp;
    *blkp = bd;
}

void Database::BuildBlockData()
{
    if(first_block_group)
        return;

    static const char blkg[] = "blockgroup";
    ScriptVector groups;
    GetSectionNames(blkg, groups);
    BlockGroupData *last = 0;
    int i;
    for(i = 0; i < groups.Length(); i++) {
        const char *gn = groups[i].c_str();
        const char *begin = inifile->GetTextParameter(blkg, gn, "begin", "");
        const char *end = inifile->GetTextParameter(blkg, gn, "end", "");
        const char *blkt =
            inifile->GetTextParameter(blkg, gn, "block_template", "");
        BlockGroupData *bgd = new BlockGroupData(gn, begin, end, blkt);
        if(last)
            last->next_grp = bgd;
        else
            first_block_group = bgd;
        last = bgd;
    }

    static const char block[] = "block";
    ScriptVector blocknames;
    GetSectionNames(block, blocknames);
    for(i = 0; i < blocknames.Length(); i++) {
        const char *bn = blocknames[i].c_str();
        const char *grpid =
            inifile->GetTextParameter(block, bn, "group", "=NOGRP=");
        int weight =
            inifile->GetIntegerParameter(block, bn, "weight", 0);
        const char *tag = inifile->GetTextParameter(block, bn, "tag", "");
        const char *title = inifile->GetTextParameter(block, bn, "title", "");
        const char *body = inifile->GetTextParameter(block, bn, "body", "");
        add_block(&first_block_group, grpid, bn, weight, tag, title, body);
    }
}


ScriptVariable Database::
BuildBlocks(const ScriptVariable &group, const ScriptVariable &aux) const
{
    const_cast<Database*>(this)->BuildBlockData();

    BlockGroupData *grp = first_block_group;
    while(grp && grp->id != group)
        grp = grp->next_grp;
    if(!grp)
        return "";

    ScriptVariable empty;
    ScriptVector auxvec(aux.IsValid() ? aux : empty, ",", " \t\r\n");

    ScriptVariable res;

    ScriptMacroprocessor sub_sub(subst);
    ScriptVector valvec;
    valvec.AddItem("title");
    valvec.AddItem("");  // [1]
    valvec.AddItem("body");
    valvec.AddItem("");  // [3]
    valvec.AddItem("id");
    valvec.AddItem("");  // [5]
    valvec.AddItem("tag");
    valvec.AddItem("");  // [7]
    sub_sub.AddMacro(new ScriptMacroDictionary("blk", valvec, false));

    BlockData *blk;
    for(blk = grp->first; blk; blk = blk->next) {
        if(blk->tag.IsInvalid() || blk->tag == "" ||
            (current_list_item_data &&
                (svec_has_elem(current_list_item_data->tags, blk->tag))) ||
            (svec_has_elem(auxvec, blk->tag)))
        {
            if(res == "")
                res += (*subst)(grp->begin);    // may be sub_sub?
            valvec[1] = (*subst)(blk->title);
            valvec[3] = (*subst)(blk->body);
            valvec[5] = blk->id;
            valvec[7] = blk->tag;
            res += sub_sub.Process(grp->block_templ);
        }
    }
    if(res != "")
        res += (*subst)(grp->end);
    return res;
}
