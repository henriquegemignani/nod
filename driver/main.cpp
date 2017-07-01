#include <stdio.h>
#include <string.h>
#include "logvisor/logvisor.hpp"
#include "nod/nod.hpp"

static void printHelp()
{
    fprintf(stderr, "Usage:\n"
                    "  nodtool extract [-f] <image-in> [<dir-out>]\n"
                    "  nodtool makegcn <fsroot-in> [<image-out>]\n"
                    "  nodtool makewii <fsroot-in> [<image-out>]\n"
                    "  nodtool mergegcn <fsroot-in> <image-in> [<image-out>]\n"
                    "  nodtool mergewii <fsroot-in> <image-in> [<image-out>]\n");
}

#if NOD_UCS2
#ifdef strcasecmp
#undef strcasecmp
#endif
#define strcasecmp _wcsicmp
#define PRISize "Iu"
int wmain(int argc, wchar_t* argv[])
#else
#define PRISize "zu"
int main(int argc, char* argv[])
#endif
{
    if (argc < 3 ||
        (!strcasecmp(argv[1], _S("makegcn")) && argc < 3) ||
        (!strcasecmp(argv[1], _S("makewii")) && argc < 3) ||
        (!strcasecmp(argv[1], _S("mergegcn")) && argc < 4) ||
        (!strcasecmp(argv[1], _S("mergewii")) && argc < 4))
    {
        printHelp();
        return 1;
    }

    /* Enable logging to console */
    logvisor::RegisterStandardExceptions();
    logvisor::RegisterConsoleLogger();

    bool verbose = false;
    nod::ExtractionContext ctx = {true,
    [&](const std::string& str, float c) {
        if (verbose)
            fprintf(stderr, "Current node: %s, Extraction %g%% Complete\n", str.c_str(), c * 100.f);
    }};
    const nod::SystemChar* inDir = nullptr;
    const nod::SystemChar* outDir = _S(".");

    for (int a=2 ; a<argc ; ++a)
    {
        if (argv[a][0] == '-' && argv[a][1] == 'f')
            ctx.force = true;
        else if (argv[a][0] == '-' && argv[a][1] == 'v')
            verbose = true;

        else if (!inDir)
            inDir = argv[a];
        else
            outDir = argv[a];
    }

    auto progFunc = [&](float prog, const nod::SystemString& name, size_t bytes)
    {
        nod::Printf(_S("\r                                                                      "));
        if (bytes != -1)
            nod::Printf(_S("\r%g%% %s %" PRISize " B"), prog * 100.f, name.c_str(), bytes);
        else
            nod::Printf(_S("\r%g%% %s"), prog * 100.f, name.c_str());
        fflush(stdout);
    };

    if (!strcasecmp(argv[1], _S("extract")))
    {
        bool isWii;
        std::unique_ptr<nod::DiscBase> disc = nod::OpenDiscFromImage(inDir, isWii);
        if (!disc)
            return 1;

        nod::Mkdir(outDir, 0755);

        nod::Partition* dataPart = disc->getDataPartition();
        if (!dataPart)
            return 1;

        if (!dataPart->extractToDirectory(outDir, ctx))
            return 1;
    }
    else if (!strcasecmp(argv[1], _S("makegcn")))
    {
        /* Pre-validate path */
        nod::Sstat theStat;
        if (nod::Stat(argv[2], &theStat) || !S_ISDIR(theStat.st_mode))
        {
            nod::LogModule.report(logvisor::Error, _S("unable to stat %s as directory"), argv[2]);
            return 1;
        }

        if (nod::DiscBuilderGCN::CalculateTotalSizeRequired(argv[2]) == -1)
            return 1;

        nod::EBuildResult ret;

        if (argc < 4)
        {
            nod::SystemString outPath(argv[2]);
            outPath.append(_S(".iso"));
            nod::DiscBuilderGCN b(outPath.c_str(), progFunc);
            ret = b.buildFromDirectory(argv[2]);
        }
        else
        {
            nod::DiscBuilderGCN b(argv[3], progFunc);
            ret = b.buildFromDirectory(argv[2]);
        }

        printf("\n");
        if (ret != nod::EBuildResult::Success)
            return 1;
    }
    else if (!strcasecmp(argv[1], _S("makewii")))
    {
        /* Pre-validate path */
        nod::Sstat theStat;
        if (nod::Stat(argv[2], &theStat) || !S_ISDIR(theStat.st_mode))
        {
            nod::LogModule.report(logvisor::Error, _S("unable to stat %s as directory"), argv[4]);
            return 1;
        }

        bool dual = false;
        if (nod::DiscBuilderWii::CalculateTotalSizeRequired(argv[2], dual) == -1)
            return 1;

        nod::EBuildResult ret;

        if (argc < 4)
        {
            nod::SystemString outPath(argv[2]);
            outPath.append(_S(".iso"));
            nod::DiscBuilderWii b(outPath.c_str(), dual, progFunc);
            ret = b.buildFromDirectory(argv[2]);
        }
        else
        {
            nod::DiscBuilderWii b(argv[3], dual, progFunc);
            ret = b.buildFromDirectory(argv[2]);
        }

        printf("\n");
        if (ret != nod::EBuildResult::Success)
            return 1;
    }
    else if (!strcasecmp(argv[1], _S("mergegcn")))
    {
        /* Pre-validate paths */
        nod::Sstat theStat;
        if (nod::Stat(argv[2], &theStat) || !S_ISDIR(theStat.st_mode))
        {
            nod::LogModule.report(logvisor::Error, _S("unable to stat %s as directory"), argv[2]);
            return 1;
        }
        if (nod::Stat(argv[3], &theStat) || !S_ISREG(theStat.st_mode))
        {
            nod::LogModule.report(logvisor::Error, _S("unable to stat %s as file"), argv[3]);
            return 1;
        }

        bool isWii;
        std::unique_ptr<nod::DiscBase> disc = nod::OpenDiscFromImage(argv[3], isWii);
        if (!disc)
        {
            nod::LogModule.report(logvisor::Error, _S("unable to open image %s"), argv[3]);
            return 1;
        }
        if (isWii)
        {
            nod::LogModule.report(logvisor::Error, _S("Wii images should be merged with 'mergewii'"));
            return 1;
        }

        if (nod::DiscMergerGCN::CalculateTotalSizeRequired(static_cast<nod::DiscGCN&>(*disc), argv[2]) == -1)
            return 1;

        nod::EBuildResult ret;

        if (argc < 5)
        {
            nod::SystemString outPath(argv[2]);
            outPath.append(_S(".iso"));
            nod::DiscMergerGCN b(outPath.c_str(), static_cast<nod::DiscGCN&>(*disc), progFunc);
            ret = b.mergeFromDirectory(argv[2]);
        }
        else
        {
            nod::DiscMergerGCN b(argv[4], static_cast<nod::DiscGCN&>(*disc), progFunc);
            ret = b.mergeFromDirectory(argv[2]);
        }

        printf("\n");
        if (ret != nod::EBuildResult::Success)
            return 1;
    }
    else if (!strcasecmp(argv[1], _S("mergewii")))
    {
        /* Pre-validate paths */
        nod::Sstat theStat;
        if (nod::Stat(argv[2], &theStat) || !S_ISDIR(theStat.st_mode))
        {
            nod::LogModule.report(logvisor::Error, _S("unable to stat %s as directory"), argv[2]);
            return 1;
        }
        if (nod::Stat(argv[3], &theStat) || !S_ISREG(theStat.st_mode))
        {
            nod::LogModule.report(logvisor::Error, _S("unable to stat %s as file"), argv[3]);
            return 1;
        }

        bool isWii;
        std::unique_ptr<nod::DiscBase> disc = nod::OpenDiscFromImage(argv[3], isWii);
        if (!disc)
        {
            nod::LogModule.report(logvisor::Error, _S("unable to open image %s"), argv[3]);
            return 1;
        }
        if (!isWii)
        {
            nod::LogModule.report(logvisor::Error, _S("GameCube images should be merged with 'mergegcn'"));
            return 1;
        }

        bool dual = false;
        if (nod::DiscMergerWii::CalculateTotalSizeRequired(static_cast<nod::DiscWii&>(*disc), argv[2], dual) == -1)
            return 1;

        nod::EBuildResult ret;

        if (argc < 5)
        {
            nod::SystemString outPath(argv[2]);
            outPath.append(_S(".iso"));
            nod::DiscMergerWii b(outPath.c_str(), static_cast<nod::DiscWii&>(*disc), dual, progFunc);
            ret = b.mergeFromDirectory(argv[2]);
        }
        else
        {
            nod::DiscMergerWii b(argv[4], static_cast<nod::DiscWii&>(*disc), dual, progFunc);
            ret = b.mergeFromDirectory(argv[2]);
        }

        printf("\n");
        if (ret != nod::EBuildResult::Success)
            return 1;
    }
    else
    {
        printHelp();
        return 1;
    }

    nod::LogModule.report(logvisor::Info, _S("Success!"));
    return 0;
}

