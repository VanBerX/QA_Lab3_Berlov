#include "database.hpp"
#include "basesubs.hpp"
#include "arrindex.hpp"

#include "dbsubst.hpp"


#if 0
NumSubstitution::NumSubstitution(int num)
{
    AddMacro(new ScriptMacroConst("n", ScriptNumber(num)));
}
#endif


void add_index_macros(ScriptMacroprocessor &smp, int idx)
{
#if 0
    if(idx == -1) {
        smp.AddMacro(new ScriptMacroConst("idx", ""));
        smp.AddMacro(new ScriptMacroConst("_idx", ""));
        smp.AddMacro(new ScriptMacroConst("idx0", ""));
        return;
    }
#endif
    ScriptNumber idxs(idx);
    ScriptVariable empty("");
    smp.AddMacro(new ScriptMacroConst("idx", idx ? idxs : empty));
    smp.AddMacro(new ScriptMacroConst("_idx", idx ?
                                  ScriptVariable("_")+idxs : empty));
    smp.AddMacro(new ScriptMacroConst("idx0", idxs));
}


#if 0
IdSubstitution::IdSubstitution(const char *id, int idx)
{
    AddMacro(new ScriptMacroConst("id", id));

    if(idx != -1) {
        ScriptNumber idxs(idx);
        ScriptVariable empty("");
        AddMacro(new ScriptMacroConst("idx", idx ? idxs : empty));
        AddMacro(new ScriptMacroConst("_idx", idx ?
                                      ScriptVariable("_")+idxs : empty));
        AddMacro(new ScriptMacroConst("idx0", idxs));
    }
}
#endif


class VarListData : public ScriptMacroprocessorMacro {
    const ListData *the_data;
public:
    VarListData() : ScriptMacroprocessorMacro("ls", true), the_data(0) {}
    void SetData(const ListData *d) { the_data = d; }
    ScriptVariable Expand(const ScriptVector &params) const;
};

ScriptVariable VarListData::Expand(const ScriptVector &params) const
{
    if(!the_data || params.Length() != 1)
        return ScriptVariableInv();
    ScriptVariable s = params[0];
    s.Trim();
    if(s == "id")
        return the_data->id;
    if(s == "srctype")
        return the_data->srctype == listsrc_ini ? "ini" : "set";
    if(s == "name" || s == "srcname")
        return the_data->srcname;
    if(s == "tag")
        return the_data->tag;
    if(s == "first")
        return the_data->items[0];
    if(s == "last")
        return the_data->items[the_data->items.Length()-1];
    return ScriptVariable("[ls:") + s + "?!]";
}

class VarListItemData : public ScriptMacroprocessorMacro {
    const Database *the_master;
    const ListItemData *the_data;
public:
    VarListItemData(const Database *m)
        : ScriptMacroprocessorMacro("li"), the_master(m), the_data(0) {}
    void SetData(const ListItemData *d);
    ScriptVariable Expand(const ScriptVector &params) const;
private:
    ScriptVariable DoExpand(const ScriptVector &params) const;

    ScriptVariableInv list_id;
    int items_per_page;
    ScriptVariableInv set_id, tag;
    void SyncListNavInfo(const ScriptVariable &lsname);

    ScriptVariableInv cached_item_id;
    int index;
    ScriptVariableInv prev_id, next_id;
    void SyncItemNavInfo();

    int DoGetWorkIndex(ScriptVariable lsname);
    ScriptVariable FindListArrayNum(const ScriptVariable &lsname) const;
    ScriptVariable GetListIndex(const ScriptVariable &lsname) const;

    bool DoGetPrevNext(ScriptVariable lsname);
    ScriptVariable GetPrevNext(const ScriptVariable &lsname, int pn) const;

    ScriptVariable FindAuxParam(ScriptVariable fnm) const;
};

void VarListItemData::SetData(const ListItemData *d)
{
    the_data = d;
}

void VarListItemData::SyncListNavInfo(const ScriptVariable &lsname)
{
    if(list_id.IsValid() && lsname == list_id)
        return;
    the_master->GetListNavigationInfo(lsname, items_per_page, set_id, tag);
}

void VarListItemData::SyncItemNavInfo()
{
    if(!the_data) {
        index = -1;
        prev_id.Invalidate();
        next_id.Invalidate();
        return;
    }
    if(cached_item_id.IsValid() && cached_item_id == the_data->item_id)
        return;
    bool ok = the_master->GetSetListIndexInfo(set_id, tag, the_data->item_id,
                                              index, prev_id, next_id);
    if(!ok) {
        index = -1;
        prev_id.Invalidate();
        next_id.Invalidate();
    }
    cached_item_id = the_data->item_id;
}

int VarListItemData::DoGetWorkIndex(ScriptVariable lsname)
{
    if(lsname.IsValid())
        lsname.Trim();
    SyncListNavInfo(lsname);

    if(lsname.IsInvalid() || lsname == "" || tag.IsInvalid() || tag == "") {
           // perhaps ini-sourced list
        return the_data->index;
    } else {
        SyncItemNavInfo();
        return index;
    }
}

ScriptVariable
VarListItemData::FindListArrayNum(const ScriptVariable &lsname) const
{
    int work_idx = const_cast<VarListItemData*>(this)->DoGetWorkIndex(lsname);
    if(items_per_page <= 0 || work_idx < 0)
        return "";
    int r = work_idx == 0 ? 1 : ((work_idx-1) / items_per_page + 1);
    return ScriptNumber(r);
}

ScriptVariable
VarListItemData::GetListIndex(const ScriptVariable &lsname) const
{
    int work_idx = const_cast<VarListItemData*>(this)->DoGetWorkIndex(lsname);
    if(work_idx < 0)
        return "";
    return ScriptNumber(work_idx);
}

bool VarListItemData::DoGetPrevNext(ScriptVariable lsname)
{
    if(lsname.IsValid())
        lsname.Trim();
    SyncListNavInfo(lsname);

    if(lsname.IsInvalid() || lsname == "" || tag.IsInvalid() || tag == "")
        return false;
    SyncItemNavInfo();
    return true;
}

ScriptVariable
VarListItemData::GetPrevNext(const ScriptVariable &lsname, int pn) const
{
    bool from_set = const_cast<VarListItemData*>(this)->DoGetPrevNext(lsname);
    if(from_set)
        return pn == 0 ? prev_id : next_id;
    return pn == 0 ? the_data->prev_id : the_data->next_id;
}

ScriptVariable VarListItemData::Expand(const ScriptVector &params) const
{
    ScriptVariable res = DoExpand(params);
    if(res.IsInvalid())
        res = "";
    return res;
}

#if 0
static ScriptVariable number_or_empty_for_negative(int x)
{
    return x < 0 ? ScriptVariable("") : ScriptNumber(x);
}

static ScriptVariable find_listarraynum(const ListItemData *itd,
                                        const Database *db,
                                        const ScriptVariable &lsname)
{
    int itemsperpage;
    ScriptVariable set_id, tag;
    db->GetListNavigationInfo(lsname, itemsperpage, set_id, tag);
    if(itemsperpage <= 0)
        return "";
    if(tag.IsInvalid() || tag == "") {   // perhaps ini-sourced list
        if(itd->index < 0)
            return "";
        int r = (itd->index-1) / itemsperpage + 1;
        return ScriptNumber(r);
    }
    int index = db->GetSetListIndex(set_id, tag, itd->item_id);
    if(index < 0)
        return "";
    int r = (index-1) / itemsperpage + 1;
    return ScriptNumber(r);
}

static ScriptVariable find_listidx(const ListItemData *itd,
                                   const Database *db,
                                   const ScriptVariable &lsname)
{
    if(lsname.IsInvalid() || lsname == "")
        return number_or_empty_for_negative(itd->index);
    int ignored;
    ScriptVariable set_id, tag;
    db->GetListNavigationInfo(lsname, ignored, set_id, tag);
    if(tag.IsInvalid() || tag == "")
        return number_or_empty_for_negative(itd->index);
    int index = db->GetSetListIndex(set_id, tag, itd->item_id);
    return number_or_empty_for_negative(index);
}
#endif

static bool string_true(const ScriptVariable &s)
{
    return s.IsValid() && s.Length() > 0;
}

ScriptVariable VarListItemData::FindAuxParam(ScriptVariable fnm) const
{
    fnm.Trim();
    int i;
    for(i = 0; i < the_data->aux_params.Length()-1; i += 2)
        if(fnm == the_data->aux_params[i])
            return the_data->aux_params[i+1];
    return ScriptVariableInv();
}

ScriptVariable VarListItemData::DoExpand(const ScriptVector &params) const
{
#if 0
    if(!the_data)
        return "==NO_LISTITEMDATA==";
    if(params.Length() < 1)
        return "==INVALID_LI_CALL==";
#endif
    if(!the_data || params.Length() < 1)
        return ScriptVariableInv();
    ScriptVariable s = params[0];
    s.Trim();
    if(s == "id")
        return the_data->item_id;
    if(s == "prev")
        return GetPrevNext(params[1], 0);
    if(s == "next")
        return GetPrevNext(params[1], 1);
    if(s == "date")
        return the_data->date;
    if(s == "title")
        return the_data->title;
    if(s == "descr")
        return the_data->descr;
    if(s == "text")
        return the_data->text;
    if(s == "tags")
        return the_data->tags.Join(", ");
    if(s == "unixtime")
        return the_data->unixtime > 0 ? ScriptNumber(the_data->unixtime) :
                                        ScriptVariable("");
    if(s == "iflong")
        return string_true(the_data->text) ? params[1] : params[2];
    if(s == "ifprev")
        return string_true(GetPrevNext(params[1], 0)) ? params[2] : params[3];
    if(s == "ifnext")
        return string_true(GetPrevNext(params[1], 1)) ? params[2] : params[3];
    if(s == "iffile") {
        params[1].Trim();
        int i;
        for(i = 0; i < the_data->files.Length(); i++)
            if(params[1] == the_data->files[i])
                return params[2];
        return params[3];
    }
    if(s == "ifmore")
        return the_data->descr.Length() < the_data->text.Length() ?
            params[1] : params[2];
    if(s == "ifcomenabled")
        return the_data->comments == "enabled" ? params[1] : params[2];
    if(s == "iflistarraynum")
        return FindListArrayNum(params[1]) != "" ? params[2] : params[3];
    if(s == "listarraynum")
        return FindListArrayNum(params[1]);
    if(s == "iflistidx")
        return GetListIndex(params[1]) != "" ?  params[2] : params[3];
    if(s == "listidx")
        return GetListIndex(params[1]);
    if(s == "hf")
        return FindAuxParam(params[1]);
    ScriptVariable apr = FindAuxParam(s);  /* XXX LEGACY! remove? */
    if(apr.IsValid())
        return apr;
    return ScriptVariable("[li:") + s + "?!]";
}


#if 0
    // this function should disappear one day, being replaced by
    // data string generation done from within The Database
    // tunable, e.g. for particular names of months, days etc.
static ScriptVariable make_date(long long date)
{
    time_t tt = date;
    struct tm *t = gmtime(&tt);
    return ScriptVariable(32, "%04d/%02d/%02d %02d:%02d",
                          t->tm_year+1900, t->tm_mon+1, t->tm_mday+1,
                          t->tm_hour, t->tm_min);
}
#endif

class VarCommentData : public ScriptMacroprocessorMacro {
    const CommentData *the_data;
public:
    VarCommentData() : ScriptMacroprocessorMacro("cmt"), the_data(0) {}
    void SetData(const CommentData *d) { the_data = d; }
    ScriptVariable Expand(const ScriptVector &params) const;
};

ScriptVariable VarCommentData::Expand(const ScriptVector &params) const
{
    if(!the_data || params.Length() < 1)
        return ScriptVariableInv();
    ScriptVariable s = params[0];
    s.Trim();
    if(s == "id")
        return ScriptNumber(the_data->id);
    if(s == "parent")
        return ScriptNumber(the_data->parent);
    if(s == "pgofparent")
        return the_data->page_of_parent_uri;
    if(s == "user")
        return the_data->user;
    if(s == "unixtime")
        return the_data->unixtime > 0 ?
            ScriptNumber(the_data->unixtime) : ScriptVariable("");
    if(s == "date")
        return the_data->date;
    if(s == "from")
        return the_data->from;
    if(s == "title")
        return the_data->title;
    if(s == "text")
        return the_data->text;
    if(s == "threadpg")
        return the_data->thread_page_uri;
    if(s == "replies")
        return the_data->replies == -1 ?
            ScriptVariable("") : ScriptNumber(the_data->replies);
    if(s == "ifroot")
        return the_data->parent == 0 ? params[1] : params[2];
    if(s == "ifparent" || s == "ifhasparent")
        return the_data->parent == 0 ? params[2] : params[1];
    if(s == "ifflag")
        return the_data->HasFlag(params[1]) ? params[2] : params[3];
    if(s == "aux") {
        if(!the_data->the_tree_aux_params || params[1].IsInvalid())
            return "";
        int i;
        for(i = 0; i < the_data->the_tree_aux_params->Length()-1; i+=2)
            if((*the_data->the_tree_aux_params)[i] == params[1])
                return (*the_data->the_tree_aux_params)[i+1];
        return "";
    }
    if(s == "hf") {
        ScriptVariable nm = params[1];
        nm.Trim();
        int len = the_data->aux_params.Length();
        int i;
        for(i = 1; i < len; i+=2)
            if(nm == the_data->aux_params[i-1])
                return the_data->aux_params[i];
        return "";
    }
    return ScriptVariable("%[cmt:") + s + ":?!]";
}



class VarHtmlSection : public ScriptMacroprocessorMacro {
    const DatabaseSubstitution *the_master;
    const Database *the_database;
public:
    VarHtmlSection(const DatabaseSubstitution *m, const Database *db)
        : ScriptMacroprocessorMacro("html"), the_master(m), the_database(db)
    {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

ScriptVariable VarHtmlSection::Expand(const ScriptVector &params) const
{
    int len = params.Length();
    if(len < 1)
        return ScriptVariableInv();
    ScriptVariable name = params[0];
    name.Trim();
    ScriptVariable sn = the_database->GetHtmlSnippet(name);
    return the_master->Process(sn, params, 1, len-1);
}



class VarOptions : public ScriptMacroprocessorMacro {
    const Database *the_database;
public:
    VarOptions(const Database *db)
        : ScriptMacroprocessorMacro("opt"), the_database(db)
    {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

ScriptVariable VarOptions::Expand(const ScriptVector &params) const
{
    int len = params.Length();
    if(len < 2)
        return ScriptVariableInv();
    ScriptVariable sect = params[0];
    ScriptVariable name = params[1];
    sect.Trim();
    name.Trim();
    return the_database->GetOption(sect, name);
}

//////////////////////////////////////////////////////////////////////////

class ArrayIndex : public ScriptMacroprocessorMacro {
    const DatabaseSubstitution *the_macroproc;
    const Database *the_database;
    IndexBarStyle style;
    ScriptVariable style_name;
public:
    ArrayIndex(const DatabaseSubstitution *sub, const Database *db)
        : ScriptMacroprocessorMacro("indexbar"),
        the_macroproc(sub), the_database(db),
        style_name(ScriptVariableInv())
    {}
    ScriptVariable Expand(const ScriptVector &params) const;
private:
    bool GetStyle(const ScriptVariable &nm);
};

ScriptVariable ArrayIndex::Expand(const ScriptVector &params) const
{
        // params[0] == style_name    params[1] == anchor (optional)
    int len = params.Length();
    if(len < 1)
        return ScriptVariableInv();
    ScriptVariable stname = params[0];
    stname.Trim();
    ScriptVariableInv anchor;
    if(len >= 2 && params[1].IsValid()) {
        anchor = params[1];
        anchor.Trim();
    }

    bool ok = const_cast<ArrayIndex*>(this)->GetStyle(stname);
    if(!ok)
        return ScriptVariableInv();

    ArrayData *ad = the_database->CurrentArrayData();
    if(!ad)
        return "";
    return build_array_index(style, *ad, *the_macroproc, anchor);
}

bool ArrayIndex::GetStyle(const ScriptVariable &nm)
{
    if(style_name.IsValid() && style_name == nm)
        return true;
    style_name = nm;
    bool ok = the_database->GetIndexBarStyle(style_name, style);
    if(!ok)
        style_name = ScriptVariableInv();
    return ok;
}

//////////////////////////////////////////////////////////////////////////

class ListInfo : public ScriptMacroprocessorMacro {
    const Database *the_database;
    ScriptVariable loaded_id;
    ListData *lsd;
public:
    ListInfo(const Database *db)
        : ScriptMacroprocessorMacro("listinfo"), the_database(db), lsd(0)
    {}
    ~ListInfo() { if(lsd) delete lsd; }
    ScriptVariable Expand(const ScriptVector &params) const;
private:
    void GetData(const ScriptVariable &nls);
};

ScriptVariable ListInfo::Expand(const ScriptVector &params) const
{
        // params[0] == function    params[1] == list id
    if(params.Length() < 2)
        return ScriptVariableInv();
    ScriptVariable s = params[0];
    s.Trim();
    ScriptVariable lsn = params[1];
    lsn.Trim();
    if(loaded_id.IsInvalid() || loaded_id != lsn)
        const_cast<ListInfo*>(this)->GetData(lsn);
    if(!lsd)  // no such list?
        return ScriptVariableInv();
    if(s == "first")
        return lsd->items[0];
    if(s == "last")
        return lsd->items[lsd->items.Length()-1];
    return ScriptVariable("[listinfo:") + s + "?!]";
}

void ListInfo::GetData(const ScriptVariable &nls)
{
    if(lsd) {
        delete lsd;
        lsd = 0;
    }
    lsd = new ListData;
    bool ok = the_database->GetListData(nls, *lsd);
    if(!ok) {
        delete lsd;
        lsd = 0;
    }
}


//////////////////////////////////////////////////////////////////////////

class Menu : public ScriptMacroprocessorMacro {
    const Database *the_database;
public:
    Menu(const Database *db)
        : ScriptMacroprocessorMacro("menu"), the_database(db)
    {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

ScriptVariable Menu::Expand(const ScriptVector &params) const
{
    int len = params.Length();
    if(len < 1)
        return ScriptVariableInv();
    ScriptVariable p0 = params[0];
    ScriptVariable p1 = params[1];
    return the_database->BuildMenu(p0.Trim(), p1.Trim());
}


class Blocks : public ScriptMacroprocessorMacro {
    const Database *the_database;
public:
    Blocks(const Database *db)
        : ScriptMacroprocessorMacro("blocks"), the_database(db)
    {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

ScriptVariable Blocks::Expand(const ScriptVector &params) const
{
    int len = params.Length();
    if(len < 1)
        return ScriptVariableInv();
    ScriptVariable p0 = params[0];
    ScriptVariable p1 = params[1];
    return the_database->BuildBlocks(p0.Trim(), p1.Trim());
}


class Embedlist : public ScriptMacroprocessorMacro {
    const Database *the_database;
public:
    Embedlist(const Database *db)
        : ScriptMacroprocessorMacro("embedlist"), the_database(db)
    {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

ScriptVariable Embedlist::Expand(const ScriptVector &params) const
{
    int len = params.Length();
    if(len < 1)
        return ScriptVariableInv();
    ScriptVariable id = params[0];
    id.Trim();

    if(!the_database->IsListEmbedded(id))
        return ScriptVariable("LIST ") + id + " CAN'T BE EMBEDDED";

    ListData listdata;
    if(!the_database->GetListData(id, listdata))
        return ScriptVariable("CONFIGURATION ERROR FOR LIST ") + id;

    Database::MacroRealm save;
    the_database->SaveMacroRealm(save);
    the_database->SetMacroData(&listdata, 0);

    ScriptVariable result;
    result += the_database->BuildListHead(listdata);

    int count = listdata.items.Length();
    bool rev = listdata.reverse;
    int i;
    for(i = rev ? count-1 : 0; rev?(i >= 0):(i < count); rev?i--:i++) {
        ListItemData itemdata;
        bool ok;


        if(listdata.srctype == listsrc_ini) {
            ok = the_database->GetListItemData(listdata, i, itemdata);
        } else {
            ok = the_database->GetSetItemDataById(listdata.srcname,
                                             listdata.items[i],
                                             &itemdata);
        }
        if(!ok) {
            result += ScriptVariable("NO DATA FOR ") +
                                     listdata.id + "/" + listdata.items[i]
                                     + " [ " + itemdata.title + " ]";
            continue;
        }
        if(itemdata.HasFlag("hidden"))
            continue;
#if 0
        // XXXXXXXXXXXXXXXXXXXXXX
        printf("*** XXX %s %d %s %s\n",
            id.c_str(), i, listdata.items[i].c_str(),
            itemdata.title.c_str());
#endif
        the_database->SetMacroData(&listdata, &itemdata);
        result += the_database->BuildListItem(listdata, itemdata);
#if 0
        printf("%d\n", result.Length());
#endif
        the_database->SetMacroData(&listdata, 0);
    }

    result += the_database->BuildListTail(listdata);
    the_database->RestoreMacroRealm(save);
    return result;
}

class BaseUri : public ScriptMacroprocessorMacro {
    const Database *the_database;
public:
    BaseUri(const Database *db)
        : ScriptMacroprocessorMacro("base_uri"), the_database(db)
    {}
    ScriptVariable Expand(const ScriptVector &ignored) const
        { return the_database->GetBaseUri(); }
};

class BaseUrl : public ScriptMacroprocessorMacro {
    const Database *the_database;
public:
    BaseUrl(const Database *db)
        : ScriptMacroprocessorMacro("base_url"), the_database(db)
    {}
    ScriptVariable Expand(const ScriptVector &ignored) const
        { return the_database->GetBaseUrl(); }
};



DatabaseSubstitution::DatabaseSubstitution(const Database *master)
    : BaseSubstitutions(""),
    vld(new VarListData), vlid(new VarListItemData(master)),
    vcmd(new VarCommentData), the_database(master)
{
        // please note that ownersip over the three 'var' objects is
        // transferred here to the base class object, and IT will delete them
    AddMacro(vlid);                             // [li: ]
    AddMacro(vld);                              // [ls: ]
    AddMacro(vcmd);                             // [cmt: ]
    AddMacro(new VarHtmlSection(this, master)); // [html: ]
    AddMacro(new VarOptions(master));           // [opt: ]
    AddMacro(new Menu(master));                 // [menu: ]
    AddMacro(new Blocks(master));               // [blocks: ]
    AddMacro(new ArrayIndex(this, master));     // [indexbar: ]
    AddMacro(new ListInfo(master));             // [listinfo: ]
    AddMacro(new Embedlist(master));            // [embedlist: ]
    AddMacro(new BaseUri(master));              // [base_uri]
    AddMacro(new BaseUrl(master));              // [base_url]
}

DatabaseSubstitution::~DatabaseSubstitution()
{
#if 0
    delete vhs;
    delete vlid;
    delete vld;
#endif
    // no deletion here! they will be deleted by the base class destructor,
    // because these objects belong to it!
}

void DatabaseSubstitution::SetData(const ListData *ld, const ListItemData *lid)
{
    vld->SetData(ld);
    vlid->SetData(lid);
}

void DatabaseSubstitution::SetCmtData(const CommentData *cmd)
{
    vcmd->SetData(cmd);
}
