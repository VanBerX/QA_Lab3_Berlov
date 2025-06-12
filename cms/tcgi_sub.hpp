#ifndef TCGI_SUB_HPP_SENTRY
#define TCGI_SUB_HPP_SENTRY

#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrmacro.hpp>

#include "cgicmsub.hpp"

class ThalassaCgiDb;
struct PathData;

class ThalassaCgiDbSubstitution : public CommonCgiSubstitutions {
    const ThalassaCgiDb *the_database;
    class VarPageData *pd;
    class VarSessionData *sd;
    class VarDiscussData *dd;
    class VarJustPostedData *jpsd;
    class VarPreviewData *prevd;
    class VarReqArg *rqa;
    ScriptVariable message;
    class IfCond *ifmessage;
    class IfCond *ifmessageok;
    class IfCond *ifactresult;
    class IfCond *ifactresultok;
public:
    ThalassaCgiDbSubstitution(const ThalassaCgiDb *master, const Cgi *cgi);
    ~ThalassaCgiDbSubstitution();

    void SetPageData(const PathData *d);
    void ForgetPageData() { SetPageData(0); }

    void SetMessage(const ScriptVariable &m,
                    bool is_it_ok, bool action_result);
    void ForgetMessage()
        { SetMessage(ScriptVariableInv(), false, false /* no matter */); }

    void SetJustPosted(const ScriptVariable &comm_id, bool hidden);
    void CurrentCommentChanged(const ScriptVariable &mq_page_id,
                               const ScriptVariable &comm_id);
    void SetPreview(const struct NewCommentData &ncd);
    void SetReqArg(const ScriptVariable &key, const ScriptVariable &val);
};

#endif
