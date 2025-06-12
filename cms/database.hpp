#ifndef DATABASE_HPP_SENTRY
#define DATABASE_HPP_SENTRY

#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrvect.hpp>

#ifndef THALASSA_LIST_INDEX_LIMIT
   /* This limits indices being read from tag files, so that
      the array of item ids can't cause troubles being resized
      to very large sizes.  Actually, this is a paranoia limit
      because these tag files are not supposed to be created by
      anything external to the site being generated.
      Items with indices over this limit are silently dropped.

      Please note the arrays are created small and are resized
      on demand, so setting this limit to huge values doesn't
      cause memory wastage, it only makes it theoretically possible
      in case there's a huge index in a tag file.

      It is most probably safe just to ignore this magic number.
    */
#define THALASSA_LIST_INDEX_LIMIT 10000
#endif

#define PAGESET_MAIN_FILE_NAME "content.txt"

enum list_source_type { listsrc_unknown = -1, listsrc_ini, listsrc_set };

struct ListData {
    ScriptVariable id;
    list_source_type srctype;
    ScriptVariable srcname, tag;   // tag is mandatory for "set",
                                   // but is presently ignored for "ini"

    bool embedded;
    int items_per_listpage;
    bool reverse;
    ScriptVariable main_listpage_name, listpage_name_templ; // %idxX% replaced
                            // defaults: id, id+"_p%idx%"

    ScriptVariable list_header, list_footer, list_item_templ;

    ScriptVector aux_params; /* the NAMES of parameters to store besides
                                "date", "title" and "descr";
                                ignored for "set" lists */

    bool pages;  // generate pages per item? (don't confuse with listpages)
    ScriptVariable itempage_templ, itempage_tail_templ;

    ScriptVariable comments_conf;

    ScriptVector items;
};

#if 0
struct NavigationHints {
    ScriptVariable key; // tag for 'set' lists... for 'ini', hints arent used
    int index;
    ScriptVariable prev, cur, next;
};
#endif

struct ListItemData {
    ScriptVariable item_id, date, title, descr, prev_id, next_id, pgtype;
    int index, idxpage;
    long long unixtime; // 0 for "not specified", -1 for "error" (not a num)
    ScriptVector tags, flags;
    ScriptVariable text;
    ScriptVariable comments;   // enabled, disabled, readonly
    ScriptVector aux_params;

    bool make_separate_directory;
    ScriptVector files;
#if 0
    NavigationHints *nav_hints;
    int nav_hints_count;
#endif
    ListItemData() : index(-1), idxpage(-1), unixtime(0)/*, nav_hints(0)*/ {}
    ~ListItemData() { /*if(nav_hints) delete[] nav_hints;*/ }
    bool HasTag(const ScriptVariable &t) const;
    bool HasFlag(const ScriptVariable &f) const;
};

enum pageset_subdir_make {
    pageset_subdir_always,
    pageset_subdir_bysource,
    pageset_subdir_never
};

struct PageSetData {
    ScriptVariable id, source_dir;

    int publish_method, make_subdirs;
    ScriptVariable setdirname;
    ScriptVariable pagedirname;
    ScriptVariable indexfilename;  // "index.html" by default
    ScriptVariable compagename;    // "c%idx%.html" by default
    ScriptVariable pagefilename;
                 // just like in lists; for the "no directory" case
    ScriptVector page_ids; // scanned up by directory scan, initially empty
    ScriptVariable comments_conf;
    ScriptVariable cmap_filename;  // none (Inv) by default
    ScriptVariable cmap_filename_nodir;
};

struct ArrayData;
struct IndexBarStyle;


struct CommentData {
    int id, parent;
    long long unixtime;
    ScriptVariable user, date, from, title, text;
    ScriptVector flags, aux_params;
    const ScriptVector *the_tree_aux_params;
    ScriptVariable page_of_parent_uri;  // filled by the generator!
    ScriptVariable thread_page_uri;     //   --//--
    int replies;                        //   --//--

    bool HasFlag(const ScriptVariable &f) const;
    CommentData() : replies(-1) {}
};



class FilterChainSet;
class ForumGenerator;

class Database {
    class IniFileParser *inifile;
    class DatabaseSubstitution *subst;
    class FilterChainMaker *filtmaker;

    ScriptVariableInv default_opt_selector;
    ScriptVector opt_selectors;

    ScriptVariableInv file_prefix, base_uri;

    const ListData *current_list_data;      // see SetListData, ForgetListData
    const ListItemData *current_list_item_data; // note: these point to locals

    ArrayData *array_data;  // not owned! may be null! owner changes it!

    struct BlockGroupData *first_block_group;

    struct SetListIndex *first_set_list;

public:

    Database();
    ~Database();

    void SetOptSelector(const ScriptVariable &whatfor /* ""/inv for default */,
                        const ScriptVariable &sel);

    bool Load(const char *filename);
    void MakeErrorMessage(ScriptVariable &msg) const;
    bool GetExtraFiles(ScriptVector &ef); // clears them, hence non-const

#if 0
    ScriptVariable GetSourcePrefix() const;
#endif

    void SetFilePrefix(ScriptVariable s);
    ScriptVariable GetFilePrefix() const;

    ScriptVariable GetSpoolDir() const;

    ScriptVariable GetBaseUri() const;
    ScriptVariable GetBaseUrl() const;

    ScriptVariable GetOption(const ScriptVariable &section,
                             const ScriptVariable &name) const;

    ScriptVariable GetHtmlSnippet(const ScriptVariable &name) const;

        // just apply generic macroexpansions such as %[html: *]
    ScriptVariable BuildGenericPart(const ScriptVariable &templ) const;

    int GetGenfiles(ScriptVector &names) const;
    int GetPages(ScriptVector &names) const;
    int GetLists(ScriptVector &names) const;
    int GetSets(ScriptVector &names) const;
    int GetCollections(ScriptVector &names) const;
    int GetBinaries(ScriptVector &names) const;
    int GetAliasSections(ScriptVector &names) const;

        /* genfiles */

    bool BuildGenericFile(const ScriptVariable &id,
                          ScriptVariable &path, int &chmod_val,
                          ScriptVariable &cont) const;

        /* pages */

    ScriptVariable GetPageFilename(const ScriptVariable &pg_id) const;
    int GetPageChmod(const ScriptVariable &pg_id) const;
    ScriptVariable BuildPage(const ScriptVariable &pg_id) const;

        /* lists */

    bool IsListEmbedded(const ScriptVariable &name) const;
    bool GetListData(const ScriptVariable &name, ListData &data) const;
    void GetListNavigationInfo(const ScriptVariable &name,
             int &per_page, ScriptVariable &set_id, ScriptVariable &tag) const;
    bool GetSetListIndexInfo(const ScriptVariable &set_id,
                             const ScriptVariable &tag,
                             const ScriptVariable &item_id,
                             int &index,
                             ScriptVariable &prev, ScriptVariable &next) const;

    bool GetListItemData(const ListData &lsdata, int idx,
                         ListItemData &data) const;

private:
    ScriptVariable GetListPgpath(const ScriptVariable &ls_id,
                                 int num) const;
public:
    ScriptVariable GetListFilename(const ScriptVariable &ls_id,
                                   int num) const;
    ScriptVariable GetListHref(const ScriptVariable &ls_id,
                               int num) const;
    ScriptVariable GetListItemFilename(const ScriptVariable &ls_id,
                                       const ScriptVariable &pg_id,
                                       int pg_idx) const;
    ScriptVariable GetListItemCommentmapFname(
               const ScriptVariable &ls_id, const ScriptVariable &pg_id) const;
    ScriptVariable GetListItemHref(const ScriptVariable &ls_id,
                                   const ScriptVariable &pg_id,
                                   int pg_idx) const;

    ScriptVariable BuildListHead(const ListData &lsd) const;
    ScriptVariable BuildListTail(const ListData &lsd) const;
    ScriptVariable BuildListItem(const ListData &lsd,
                                 const ListItemData &itd) const;
private:
    ScriptVariable BuildListItemPart(const ListData &lsd,
                                     const ListItemData &itd,
                                     const ScriptVariable &templ) const;
public:
#if 0
    ScriptVariable BuildListSegment(const ListData &lsd,
                                    const ScriptVariable &tag,
                                    const ScriptVariable &templ) const;
#endif

    void BuildListItemPage(const ListData &lsd, const ListItemData &itd,
                           ScriptVariable &main, ScriptVariable &tail,
                           ForumGenerator **fg) const;

#if 0
    void FillArrayDataForList(const ListData &lsd, ArrayData &ad) const;

    void FillArrayDataForListItem(const ListData &lsd,
                                  const ListItemData &itd,
                                  ForumGenerator *fg,
                                  ArrayData &ad) const;
#endif



        /* sets */

private:
    ScriptVariable GetSetSourceDir(const ScriptVariable &set_id) const;
public:
    bool GetSetData(const ScriptVariable &set_id, PageSetData &data) const;
    void ScanSetDirectory(PageSetData &data) const;
    bool ScanSetTagItems(const ScriptVariable &set_id,
                         const ScriptVariable &tag,
                         ScriptVector &items) const;
    bool GetSetItemData(const PageSetData &setd, int idx,
                        ListItemData *itd) const;
    bool GetSetItemDataById(const ScriptVariable &set_id,
                            const ScriptVariable &page_id,
                            ListItemData *itd) const;
    bool GetSetItemSource(const ScriptVariable &set_id,
                          const ScriptVariable &page_id,
                          ScriptVariable &srcd, ScriptVariable &fname,
                          bool &dedic_dir) const;

    void GetSetItemFilenames(const PageSetData &setd, bool separ_dir,
                             ScriptVariable &dir, ScriptVariable &idxfl,
                             ScriptVariable &href, ScriptVariable &cmap) const;
    void GetSetSubitemFilename(const PageSetData &setd, bool sepd, int subidx,
                               ScriptVariable &flname,
                               ScriptVariable &href) const;

    void BuildSetItemPage(const PageSetData &lsd, const ListItemData &itd,
                          ScriptVariable &main, ScriptVariable &tail,
                          ForumGenerator **fg) const;

        /* collections */

    bool GetCollectionData(const ScriptVariable &id,
                           ScriptVariable &srcdir, ScriptVariable &dstdir,
                           int &method) const;

        /* binaries */

    bool GetBinaryData(const ScriptVariable &id,
                       ScriptVariable &src, ScriptVariable &dst,
                       int &method) const;

        /* aliases */

    void GetAliases(const ScriptVariable &id, ScriptVector &res) const;
    void GetAliasDirs(const ScriptVariable &id, ScriptVector &res) const;
    ScriptVariable GetAliasDirFilename(const ScriptVariable &id) const;
    ScriptVariable BuildAliasDirFile(const ScriptVariable &id,
                               const ScriptVariable &target_uri) const;


        /* array index bars */

    bool GetIndexBarStyle(const ScriptVariable &name, IndexBarStyle &s) const;

#if 0
    ScriptVariable BuildArrayIndex(const ScriptVariable &stylename,
                                   const ScriptVariable &anchor) const;
#endif

        // returns the previously-installed pointer
    ArrayData *InstallArrayData(ArrayData *p);
    ArrayData *CurrentArrayData() const { return array_data; }

        /* menus */

    ScriptVariable BuildMenu(const ScriptVariable &id,
                             const ScriptVariable &cur_item) const;

        /* comments */

#if 0
    ScriptVariable BuildCommentSection(ForumGenerator *fg,
                                       const ScriptVariable &href,
                                       ScriptVariable &cmtmap) const;
#endif

                         // used by ForumGen's subclasses, hence public
    ScriptVariable BuildCommentPart(const CommentData &cdt,
                                    const ScriptVariable &templ) const;

            // for the update/... commands
            // be sure to set the current ListItemData!
    ScriptVariable GetCommentsPath(ScriptVariable conf) const;

        /* blocks */

    ScriptVariable BuildBlocks(const ScriptVariable &group,
                               const ScriptVariable &forced_blocks) const;


        /* local macro expansion control */

    void SetMacroData(const ListData *ld, const ListItemData *lid = 0) const;
    void ForgetMacroData() const;

    struct MacroRealm {
        const ListData *ld;
        const ListItemData *lid;
    };
    void SaveMacroRealm(MacroRealm &buf) const;
    void RestoreMacroRealm(const MacroRealm &buf) const;

private:
    int GetSectionNames(const char *gn, class ScriptVector &names) const;
#if 0
    bool GetTextMaybeFile(const char *g, const char *s, const char *p,
                          ScriptVariable &res) const;
#endif
    ScriptVariable ExpandTemplate(const char *templ,
                          const char *g, const char *s) const;

    ForumGenerator *GetComments(ScriptVariable conf) const;

    void ProvideFilterChainMaker() const;

    void BuildBlockData();

    SetListIndex *ProvideSetListIndex(const ScriptVariable &set_id,
                                      const ScriptVariable &tag);

public:
    FilterChainSet *MakeFormatFilter(const class HeadedTextMessage &msg) const;

};

#endif
