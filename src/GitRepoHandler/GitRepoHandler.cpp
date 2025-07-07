#include "GitRepoHandler/GitRepoHandler.h"

GitRepoHandler::GitRepoHandler () { git_libgit2_init(); }
GitRepoHandler::~GitRepoHandler () { git_libgit2_shutdown(); }

bool GitRepoHandler::isUpToDate ()
{
    git_repository* repo = nullptr;
    int error = git_repository_open(&repo, repoPath.c_str());
    if(error != 0)
    {
        std::cerr << "Lokales Repo nicht gefunden oder konnte nicht geöffnet werden\n";
        return false;
    }

    git_remote* remote = nullptr;
    error = git_remote_lookup(&remote, repo, "origin");
    if(error != 0)
    {
        std::cerr << "Remote 'origin' nicht gefunden\n";
        git_repository_free(repo);
        return false;
    }

    git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
    error = git_remote_fetch(remote, nullptr, &fetch_opts, nullptr);
    if(error != 0)
    {
        std::cerr << "Fehler beim git fetch\n";
        git_remote_free(remote);
        git_repository_free(repo);
        return false;
    }

    git_reference* head_ref = nullptr;
    error = git_repository_head(&head_ref, repo);
    if(error != 0)
    {
        std::cerr << "HEAD konnte nicht gefunden werden\n";
        git_remote_free(remote);
        git_repository_free(repo);
        return false;
    }

    const git_oid* local_oid_ptr = git_reference_target(head_ref);
    if(local_oid_ptr == nullptr)
    {
        std::cerr << "HEAD ist kein direkter Ref\n";
        git_reference_free(head_ref);
        git_remote_free(remote);
        git_repository_free(repo);
        return false;
    }
    git_oid local_oid = *local_oid_ptr;

    git_oid remote_oid;
    error = git_reference_name_to_id(&remote_oid, repo, "refs/remotes/origin/main");
    if(error != 0)
    {
        error = git_reference_name_to_id(&remote_oid, repo, "refs/remotes/origin/master");
        if(error != 0)
        {
            std::cerr << "Remote-Branch 'origin/main' oder 'origin/master' nicht gefunden\n";
            git_reference_free(head_ref);
            git_remote_free(remote);
            git_repository_free(repo);
            return false;
        }
    }

    bool isSame = (git_oid_cmp(&local_oid, &remote_oid) == 0);

    git_reference_free(head_ref);
    git_remote_free(remote);
    git_repository_free(repo);

    return isSame;
}

bool GitRepoHandler::cloneRepo ()
{
    git_repository* repo = nullptr;
    int error = git_clone(&repo, remoteUrl.c_str(), repoPath.c_str(), nullptr);
    if(error != 0)
    {
        const git_error* e = git_error_last();
        std::cerr << "Fehler beim Klonen: " << (e ? e->message : "Unbekannter Fehler") << std::endl;
        return false;
    }
    git_repository_free(repo);
    return true;
}

bool GitRepoHandler::checkUpdates ()
{
    git_repository* repo = nullptr;
    int error = git_repository_open(&repo, repoPath.c_str());
    if(error != 0)
    {
        std::cout << "Kein Repo gefunden, klone es...\n";
        if(!cloneRepo())
        {
            return false;
        }
    }
    else
    {
        git_repository_free(repo);
    }

    return isUpToDate();
}
