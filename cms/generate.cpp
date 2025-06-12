#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrvect.hpp>
#include <scriptpp/cmd.hpp>

#include "database.hpp"
#include "forumgen.hpp"
#include "arrindex.hpp"
#include "errlist.hpp"
#include "fileops.hpp"
#include "fpublish.hpp"

#include "generate.hpp"

// * architecture decision *
// PLEASE REMEMBER!!!
// all substitutions must (yes, MUST) be done within the database object,
// because (!) variable names are a part of the database format!




static FILE* start_file(const ScriptVariable &fname,
                        int chmod_val,
                        const ScriptVariable &diag_id,
                        ErrorList **err)
{
    make_directory_path(fname.c_str(), 1);
    FILE* f = fopen(fname.c_str(), "w");
    if(!f) {
        ScriptVariable s(63, "Can't create file %s for %s [%s], skipping",
                         fname.c_str(), diag_id.c_str(), strerror(errno));
        ErrorList::AddError(err, s);
        return 0;
    }
    if(chmod_val)
        fchmod(fileno(f), chmod_val);
    return f;
}

static bool output_file(const ScriptVariable &fname,
                        int chmod_val,
                        const ScriptVariable &diag_id,
                        ErrorList **err,
                        const char *s1, const char *s2 = 0, const char *s3 = 0)
{
    FILE *f = start_file(fname, chmod_val, diag_id, err);
    if(!f)
        return false;
    fputs(s1, f);
    if(s2)
        fputs(s2, f);
    if(s3)
        fputs(s3, f);
    fclose(f);
    return true;
}


//////////////////////////////////////////////////////////////////////////
// Genfiles
// defined by ''genfile'' ini section, generated outside of the doctree
//

void generate_genfile(const ScriptVariable &id, Database& database,
                      ErrorList **err)
{
    ScriptVariable path, content;
    int chmod_val;
    if(!database.BuildGenericFile(id, path, chmod_val, content)) {
        ScriptVariable s(63, "Skipping generic file %s, check configs",
                             id.c_str());
        ErrorList::AddError(err, s);
        return;
    }
    ScriptVariable diag_id("-");
    output_file(path, chmod_val, diag_id, err, content.c_str());
}

void generate_all_genfiles(Database& database, ErrorList **err)
{
    ScriptVector gfnames;
    database.GetGenfiles(gfnames);
    int i;
    for(i = 0; i < gfnames.Length(); i++) {
         generate_genfile(gfnames[i], database, err);
    }
}


//////////////////////////////////////////////////////////////////////////
// Pages
// defined by ''page'' ini section, which includes all needed data
//
// pages can NOT provide a comment forum
//

bool generate_page(const ScriptVariable& id, Database& database,
                   ErrorList **err)
{
    ScriptVariable page_filename;
    page_filename = database.GetPageFilename(id);
    if(page_filename.IsInvalid() || page_filename == "")
        return true;  /* suppressed, but it's not an error */
    ScriptVariable s = database.BuildPage(id);
    if(s.IsInvalid()) {
        ScriptVariable s(63, "Page %s not configured", id.c_str());
        ErrorList::AddError(err, s);
        return false;
    }
    int chmod_val = database.GetPageChmod(id);
    ScriptVariable diag_id("page ");
    diag_id += id;

    return output_file(page_filename, chmod_val, diag_id, err, s.c_str());
}

void generate_all_pages(Database& database, ErrorList **errlst)
{
    ScriptVector pagenames;
    database.GetPages(pagenames);
    int i;
    for(i = 0; i < pagenames.Length(); i++) {
        generate_page(pagenames[i], database, errlst);
    }
}

//////////////////////////////////////////////////////////////////////////
// Lists
// defined by ''list'' ini section,
// represented by ini section group named within the pagelist section
//

static void output_list_head(FILE *listf,
                             const ListData &listdata,
                             Database &database,
                             ErrorList **err)
{
    ScriptVariable s = database.BuildListHead(listdata);
    fputs(s.c_str(), listf);
}

static void output_list_tail(FILE *listf, const ListData &listdata,
                             Database &database,
                             ErrorList **err)
{
    ScriptVariable s = database.BuildListTail(listdata);
    fputs(s.c_str(), listf);
}

static void output_list_item(FILE *listf,
                             const ListData &listdata,
                             const ListItemData &itemdata,
                             Database &database,
                             ErrorList **err)
{
    ScriptVariable s = database.BuildListItem(listdata, itemdata);
    fputs(s.c_str(), listf);
}

static void generate_list_item_page(const ListData &listdata,
                                    const ListItemData &itemdata,
                                    Database &database,
                                    ErrorList **err)
{
    ScriptVariable diag_id = listdata.id + "::" + itemdata.item_id;

    ScriptVariable mainpg, tail;
    ForumGenerator *fg;
    database.BuildListItemPage(listdata, itemdata, mainpg, tail, &fg);

    int pgcnt;
    bool reverse;
    bool multipage_by_comments = false;
    if(fg) {
        bool ok = fg->GetArrayInfo(pgcnt, reverse);
        if(ok && pgcnt > 1)
            multipage_by_comments = true;
    }

    if(!multipage_by_comments) {
        // very simple case
        ArrayData *save = database.InstallArrayData(0);
        ScriptVariable fname =
            database.GetListItemFilename(listdata.id, itemdata.item_id, 0);
        ScriptVariable cmts("");
        if(fg)
           cmts = fg->Build();
        output_file(fname, 0, diag_id, err,
                    mainpg.c_str(), cmts.c_str(), tail.c_str());
        database.InstallArrayData(save);
        if(fg)
            delete fg;
        return;
    }

    ScriptVariable href_main =
        database.GetListItemHref(listdata.id, itemdata.item_id, 0);

    ////////////////////////////
    ArrayData arrayd;
    arrayd.page_count = pgcnt;
    arrayd.reverse = reverse;
    arrayd.href_array[0] = href_main;
    int i;
    for(i = 1; i <= pgcnt; i++) {
        int idxval = fg->GetPageIdxValue(i);
        arrayd.href_array[i] =
            database.GetListItemHref(listdata.id, itemdata.item_id, idxval);
    }
    if(reverse) {
        arrayd.href_last = href_main;
    } else {
        arrayd.href_first = href_main;
        arrayd.href_array[1] = href_main;
    }
    arrayd.current_page = -1;
    ArrayData *ad_save = database.InstallArrayData(&arrayd);
    ////////////////////////////

    fg->SetMainHref(href_main);

    bool ok;
    do {
        int idxf, idx_array;
        fg->GetIndices(idxf, idx_array);
        if(idx_array < 0 || idx_array > pgcnt)
            idx_array = 0;
        arrayd.current_page = idx_array;
        fg->SetHref(arrayd.href_array[idx_array]);
        ScriptVariable compage = fg->Build();
        ScriptVariable fname =
            database.GetListItemFilename(listdata.id, itemdata.item_id, idxf);
        output_file(fname, 0, diag_id, err,
                    mainpg.c_str(), compage.c_str(), tail.c_str());
        ok = fg->Next();
    } while(ok);

    ScriptVariable cmap = fg->MakeCommentMap();
    delete fg;

    database.InstallArrayData(ad_save);

    if(cmap.IsValid() && cmap != "") {
        ScriptVariable cmap_fname =
            database.GetListItemCommentmapFname(listdata.id, itemdata.item_id);
        if(cmap_fname.IsValid())
            output_file(cmap_fname, 0, diag_id, err, cmap.c_str());
    }
}

static void generate_all_list_item_pages(const ListData &list_data,
                                Database& database, ErrorList **err)
{
    int max = list_data.items.Length();
    int i;
    for(i = 0; i < max; i++) {
        ListItemData itemdata;
        if(!database.GetListItemData(list_data, i, itemdata)) {
            ErrorList::AddError(err, ScriptVariable("No data for ") +
                                     list_data.id + "/" + list_data.items[i]);
            continue;
        }
        database.SetMacroData(&list_data, &itemdata);
        generate_list_item_page(list_data, itemdata, database, err);
        database.ForgetMacroData();
    }
}

void generate_single_list_item_page(const ScriptVariable &list_id,
                                    const ScriptVariable &item_id,
                                    Database &database,
                                    ErrorList **err)
{
    ListData list_data;
    if(!database.GetListData(list_id, list_data)) {
        ScriptVariable s(63, "Config. error for list %s\n", list_id.c_str());
        ErrorList::AddError(err, s);
        return;
    }
    if(!list_data.pages) {
        ScriptVariable s(63, "List %s has no item pages\n", list_id.c_str());
        ErrorList::AddError(err, s);
        return;
    }

    int idx = -1;
    int max = list_data.items.Length();
    int i;
    for(i = 0; i < max; i++) {
        if(list_data.items[i] == item_id) {
            idx = i;
            break;
        }
    }
    if(idx == -1) {
        ScriptVariable s(63, "Item %s not found in list %s\n",
                             item_id.c_str(), list_id.c_str());
        ErrorList::AddError(err, s);
        return;
    }

    ListItemData itemdata;
    bool ok = database.GetListItemData(list_data, idx, itemdata);
    if(!ok) {
        ErrorList::AddError(err, ScriptVariable("No data for ") +
                                 list_data.id + "/" + list_data.items[i]);
        return;
    }
    database.SetMacroData(&list_data, &itemdata);
    generate_list_item_page(list_data, itemdata, database, err);
    database.ForgetMacroData();
}

static void generate_empty_list(const ListData &list_data,
                                const ScriptVariable &filename,
                                const ScriptVariable &diag_id,
                                Database& database, ErrorList **err)
{
    FILE *listf = start_file(filename, 0, diag_id, err);
    if(!listf)
        return;

    database.SetMacroData(&list_data, 0);
    output_list_head(listf, list_data, database, err);
    output_list_tail(listf, list_data, database, err);
    database.ForgetMacroData();

    fclose(listf);
}

static void generate_list_segment(const ListData &list_data,
                                  const ScriptVariable &filename,
                                  int pgnum, int startidx, int endidx,
                                  const ScriptVariable &diag_id,
                                  Database& database, ErrorList **err)
{
    FILE *listf = start_file(filename, 0, diag_id, err);
    if(!listf)
        return;

    database.SetMacroData(&list_data, 0);
    output_list_head(listf, list_data, database, err);

    int order = startidx < endidx ? 1 : -1;
    int i;
    for(i = startidx; (order==1) ? (i<=endidx) : (i>=endidx); i += order) {
        ListItemData itemdata;
        bool ok;

        if(list_data.srctype == listsrc_ini) {
            ok = database.GetListItemData(list_data, i, itemdata);
        } else {
            ok = database.GetSetItemDataById(list_data.srcname,
                                             list_data.items[i],
                                             &itemdata);
        }
        if(!ok) {
            ErrorList::AddError(err, ScriptVariable("No data for ") +
                                     list_data.id + "/" + list_data.items[i]
                                     + " [ " + itemdata.title + " ]");
            continue;
        }
        if(itemdata.HasFlag("hidden"))
            continue;
        database.SetMacroData(&list_data, &itemdata);
        output_list_item(listf, list_data, itemdata, database, err);
        database.SetMacroData(&list_data, 0);
    }

    output_list_tail(listf, list_data, database, err);
    database.ForgetMacroData();

    fclose(listf);
}

static int start_num_for_main_list_page(int itemcnt, int perpage)
{
    if(perpage == 0)
        return 0;
    if(itemcnt < 2 * perpage)
        return 0;
//    return itemcnt - (itemcnt % perpage + perpage);
    return itemcnt - perpage;
}

template <class T>
void swapvars(T &a, T&b)
{
    T c = a;
    a = b;
    b = c;
}

#if 0
static ScriptVariable make_uri(ScriptVariable &base, const ScriptVariable &u)
{
    bool need_slash = base.Range(-1, 1)[0] != '/' && u[0] != '/';
    const char *sep = need_slash ? "/" : "";
    return base + sep + u;
}
#endif

void generate_list(const ScriptVariable& id, Database& database,
                   ErrorList **err)
{
    int i;
    ListData list_data;
    if(!database.GetListData(id, list_data)) {
        ScriptVariable s(63, "Configuration error for list %s\n", id.c_str());
        ErrorList::AddError(err, s);
        return;
    }
    if(list_data.pages)
        generate_all_list_item_pages(list_data, database, err);

    if(list_data.embedded)
        return;

    ScriptVariable diag_id = "list ";
    diag_id += id;

    bool rev = list_data.reverse;
    int perpage = list_data.items_per_listpage;
    int itemcnt = list_data.items.Length();
    int pagecnt = perpage <= 0 ? 1 : (itemcnt ? (itemcnt-1)/perpage+1 : 0);
    ////////////////////////////////
    ArrayData array_data;
    array_data.page_count = pagecnt;
    array_data.reverse = rev;
    for(i = 0; i <= pagecnt; i++)
        array_data.href_array[i] = database.GetListHref(id, i);
    if(rev)
        array_data.href_last = array_data.href_array[0];
    else
        array_data.href_first = array_data.href_array[0];
    array_data.current_page = rev ? -1 : 1;
    ////////////////////////////////
    ArrayData *ad_save = database.InstallArrayData(&array_data);

    ScriptVariable filename;
    filename = database.GetListFilename(id, -1);

    if(itemcnt < 1) {
            // this case is special because parameters of
            // generate_list_segment don't allow to specify
            // zero-length segment
        generate_empty_list(list_data, filename, diag_id, database, err);
    } else {
        int start_num = rev ? itemcnt - 1 : 0;
        int end_num =
            rev ?
                start_num_for_main_list_page(itemcnt, perpage) :
                (perpage > 0 ? perpage-1 : itemcnt-1);
        generate_list_segment(list_data, filename, 0, start_num, end_num,
                              diag_id, database, err);
        if(pagecnt > 1 || (pagecnt == 1 && perpage > 0 && rev)) {
                 // reverse multipage with only one page is special case
                 // to make the ``permanent links'' work
                                        // otherwise, we're already done
            for(i = rev ? 1 : 2; i <= pagecnt; i++) {
                array_data.current_page = i;
                start_num = (i - 1) * perpage;
                end_num = i * perpage - 1;
                if(end_num >= itemcnt)
                    end_num = itemcnt - 1;
                if(rev)
                    swapvars(start_num, end_num);
                filename = database.GetListFilename(id, i);
                generate_list_segment(list_data, filename, i,
                                      start_num, end_num,
                                      diag_id, database, err);
            }
        }
    }
    database.InstallArrayData(ad_save);
}

void generate_all_lists(Database& database, ErrorList **errlst)
{
    ScriptVector listnames;
    database.GetLists(listnames);
    int i;
    for(i = 0; i < listnames.Length(); i++)
        generate_list(listnames[i], database, errlst);
}




//////////////////////////////////////////////////////////////////////////
// Sets
// defined by 'pageset' section
// represented with a directory containing files and/or subdirs, one per page


static void build_set_page(const PageSetData &data, int idx,
                           Database& database, ErrorList **err)
{
    ScriptVariable whatfor = data.id + "::" + data.page_ids[idx];

    ListItemData lid;
    if(!database.GetSetItemData(data, idx, &lid)) {
        ScriptVariable s(63, "Can't get set page data %s, skipping",
                         whatfor.c_str());
        ErrorList::AddError(err, s);
        return;
    }

    if(lid.HasFlag("hidden"))
        return;

    database.SetMacroData(0, &lid);

    bool separ_dir;
    switch(data.make_subdirs) {
    case pageset_subdir_always:
        separ_dir = true;
        break;
    case pageset_subdir_never:
        separ_dir = false;
        break;
    case pageset_subdir_bysource:
    default:
        separ_dir = lid.make_separate_directory;
    }

    ScriptVariable destdir, destidx, main_href, cmapfname;
    database.GetSetItemFilenames(data, separ_dir,
                                 destdir, destidx, main_href, cmapfname);

    if(!provide_dest_dir(destdir, whatfor, err))
        return;

    ScriptVariable mainpg, tail;
    ForumGenerator *fg;
    database.BuildSetItemPage(data, lid, mainpg, tail, &fg);

    int pgcnt;
    bool reverse;
    bool multipage_by_comments = false;
    if(fg) {
        bool ok = fg->GetArrayInfo(pgcnt, reverse);
        if(ok && pgcnt > 1)
            multipage_by_comments = true;
    }

    if(!multipage_by_comments) {
        // very simple case
        ArrayData *save = database.InstallArrayData(0);
        ScriptVariable cmts("");
        if(fg)
            cmts = fg->Build();
        output_file(destidx, 0, whatfor, err,
                    mainpg.c_str(), cmts.c_str(), tail.c_str());
        database.InstallArrayData(save);
    } else {
        ////////////////////////////
        ArrayData arrayd;
        arrayd.page_count = pgcnt;
        arrayd.reverse = reverse;
        arrayd.href_array[0] = main_href;

        ScriptVector sub_filename;
        sub_filename[0] = destidx;

        int i;
        for(i = 1; i <= pgcnt; i++) {
            int idxval = fg->GetPageIdxValue(i);
            ScriptVariable sfn, hrf;
            database.GetSetSubitemFilename(data, separ_dir, idxval, sfn, hrf);
            sub_filename[i] = sfn;
            arrayd.href_array[i] = hrf;
        }
        if(reverse)
            arrayd.href_last = main_href;
        else
            arrayd.href_first = main_href;
        arrayd.current_page = -1;
        ArrayData *ad_save = database.InstallArrayData(&arrayd);
        ////////////////////////////

        fg->SetMainHref(main_href);

        bool ok;
        do {
            int idxf, idx_array;
            fg->GetIndices(idxf, idx_array);
            if(idx_array < 0 || idx_array > pgcnt)
                idx_array = 0;
            arrayd.current_page = idx_array;
            fg->SetHref(arrayd.href_array[idx_array]);
            ScriptVariable compage = fg->Build();
            output_file(sub_filename[idx_array], 0, whatfor, err,
                        mainpg.c_str(), compage.c_str(), tail.c_str());
            ok = fg->Next();
        } while(ok);

        database.InstallArrayData(ad_save);

        ScriptVariable cmap = fg->MakeCommentMap();
        if(cmap.IsValid() && cmap != "" && cmapfname.IsValid())
            output_file(cmapfname, 0, whatfor, err, cmap.c_str());
    }
    if(fg)
        delete fg;

    publish_files(data.source_dir + "/" + data.page_ids[idx], destdir,
                  lid.files, data.publish_method, whatfor, err);

    database.ForgetMacroData();
}

void generate_single_set_page(const ScriptVariable& set_id,
                              const ScriptVariable& page_id,
                              Database& database,
                              ErrorList **err)
{
    PageSetData data;
    if(!database.GetSetData(set_id, data)) {
        ErrorList::AddError(err, ScriptVariable("No data for ") + set_id);
        return;
    }
    data.page_ids[0] = page_id;
    build_set_page(data, 0, database, err);
}

void generate_set(const ScriptVariable& id, Database& database,
                  ErrorList **err)
{
    PageSetData data;
    if(!database.GetSetData(id, data)) {
        ErrorList::AddError(err, ScriptVariable("No data for ") + id);
        return;
    }
    database.ScanSetDirectory(data);
    int pgcount = data.page_ids.Length();
    int i;
    for(i = 0; i < pgcount; i++)
        build_set_page(data, i, database, err);
}


void generate_all_sets(Database& database, ErrorList **errlst)
{
    ScriptVector setnames;
    database.GetSets(setnames);
    int i;
    for(i = 0; i < setnames.Length(); i++) {
        generate_set(setnames[i], database, errlst);
    }
}


//////////////////////////////////////////////////////////////////////////
// Collections
// Defined by [collection id] sections
// Generally, it is just a directory with files

void publish_collection(const ScriptVariable& id,
                        Database& database, ErrorList **err)
{
    ScriptVariable srcdir, dstdir;
    int method;

    database.GetCollectionData(id, srcdir, dstdir, method);
    publish_dir(srcdir, dstdir, method, id, err);
}

void publish_all_collections(Database& database, ErrorList **errlst)
{
    ScriptVector collnames;
    database.GetCollections(collnames);
    int i;
    for(i = 0; i < collnames.Length(); i++) {
        publish_collection(collnames[i], database, errlst);
    }
}


//////////////////////////////////////////////////////////////////////////
// Binaries
// Defined by [binary name] sections
// It is a single file (not necessarily binary) of arbitrary nature

void publish_binary(const ScriptVariable& id, Database& database,
                    ErrorList **err)
{
    ScriptVariable src, dst;
    int method;

    database.GetBinaryData(id, src, dst, method);

    ScriptVariable whatfor("binary ");
    whatfor += id;

    if(src.IsInvalid() || src == "")
        src = id;
    else
    if(src.Range(-1, 1)[0] == '/')
        src += id;
    else {
        FileStat fs(src.c_str());
        if(!fs.Exists()) {
            ScriptVariable s(63, "Source %s doesn't exist [%s]",
                             src.c_str(), whatfor.c_str());
            ErrorList::AddError(err, s);
            return;
        }
        if(fs.IsDir()) {
            src += '/';
            src += id;
        }
    }
    publish_given_file(src, dst, method, whatfor, err);
}

void publish_all_binaries(Database& database, ErrorList **errlst)
{
    ScriptVector binnames;
    database.GetBinaries(binnames);
    int i;
    for(i = 0; i < binnames.Length(); i++) {
        publish_binary(binnames[i], database, errlst);
    }
}

//////////////////////////////////////////////////////////////////////////
// Alias sections
// Defined by [aliases id] sections
// In the present version, these are implemented as symlinks, except for
// the directories explicitly listed, which are implemented using a
// file (either .htaccess, or index.html that causes redirect)
//

static void generate_single_alias(const ScriptVariable &alias,
                                  const ScriptVariable &original,
                                  const ScriptVariable &whatfor,
                                  Database& database,
                                  ErrorList **err)
{
    int r = make_symlink(original, alias);
    if(r == -1) {
        ScriptVariable s(127, "couldn't alias %s to %s [%s]",
                        alias.c_str(), original.c_str(), whatfor.c_str());
        ErrorList::AddError(err, s);
    }
}

static void form_aliased_directory(const ScriptVariable &dirpath,
                                   const ScriptVariable &target_uri,
                                   const ScriptVariable &sect_id,
                                   Database& database,
                                   ErrorList **err)
{
    make_directory_path(dirpath.c_str(), 0);
    ScriptVariable fname = database.GetAliasDirFilename(sect_id);
    ScriptVariable body = database.BuildAliasDirFile(sect_id, target_uri);
    output_file(dirpath + "/" + fname, 0, sect_id, err, body.c_str());
}

static bool svec_has_elem(const ScriptVector &v, const ScriptVariable &el)
{
    int i;
    for(i = 0; i < v.Length(); i++) {
        if(v[i] == el)
            return true;
    }
    return false;
}

void generate_aliases(const ScriptVariable& id, Database& database,
                      ErrorList **err)
{
    ScriptVariable fbase = database.GetFilePrefix();
    ScriptVector aliasvec, forced_dirs;
    database.GetAliases(id, aliasvec);
    database.GetAliasDirs(id, forced_dirs);
    int i;
    int n = aliasvec.Length();
    for(i = 0; i < n - 1; i+=2) {
        ScriptVariable src = aliasvec[i];
        ScriptVariable dst = aliasvec[i+1];
        if(svec_has_elem(forced_dirs, src)) {
            form_aliased_directory(fbase + src, dst, id, database, err);
        } else {
            generate_single_alias(fbase + src, fbase + dst, id, database, err);
        }
    }
}

void generate_all_alias_sections(Database& database, ErrorList **errlst)
{
    ScriptVector names;
    database.GetAliasSections(names);
    int i;
    for(i = 0; i < names.Length(); i++) {
        generate_aliases(names[i], database, errlst);
    }
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


    // database isn't const, 'cause generate_list affects the object
ErrorList* generate_everything(Database& database)
{
    ErrorList *errls = 0;

    make_directory_path(database.GetFilePrefix().c_str(), 0);

    generate_all_genfiles(database, &errls);
    generate_all_pages(database, &errls);
    publish_all_collections(database, &errls);
    publish_all_binaries(database, &errls);

    generate_all_lists(database, &errls);
    generate_all_sets(database, &errls);
       /* _sets should be after _lists to have access to some list info */

    generate_all_alias_sections(database, &errls);

    return errls;
}
