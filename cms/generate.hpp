#ifndef GENERATE_HPP_SENTRY
#define GENERATE_HPP_SENTRY

class Database;
class ErrorList;

bool generate_page(const ScriptVariable &id, Database &database,
                   ErrorList **err);
void generate_all_pages(Database &database, ErrorList **err);

void generate_genfile(const ScriptVariable &id, Database &database,
                      ErrorList **err);
void generate_all_genfiles(Database &database, ErrorList **err);

void publish_collection(const ScriptVariable &id, Database &database,
                        ErrorList **err);
void publish_all_collections(Database &database, ErrorList **err);

void publish_binary(const ScriptVariable &id, Database &database,
                    ErrorList **err);
void publish_all_binaries(Database& database, ErrorList **err);

void generate_single_list_item_page(const ScriptVariable &list_id,
                                    const ScriptVariable &item_id,
                                    Database &database,
                                    ErrorList **err);
void generate_list(const ScriptVariable &id, Database &database,
                   ErrorList **err);
void generate_all_lists(Database &database, ErrorList **err);

void generate_single_set_page(const ScriptVariable &set_id,
                              const ScriptVariable &page_id,
                              Database &database,
                              ErrorList **err);
void generate_set(const ScriptVariable &id, Database &database,
                  ErrorList **err);
void generate_all_sets(Database &database, ErrorList **err);

void generate_aliases(const ScriptVariable &id, Database &database,
                      ErrorList **err);
void generate_all_alias_sections(Database &database, ErrorList **err);

struct ErrorList *generate_everything(Database &database);

#endif
