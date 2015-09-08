#include <boost/property_tree/ptree.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/optional.hpp>

#include <regex>
#include <vector>

using namespace boost::property_tree;
namespace bfs = boost::filesystem;

class SProjectInfo
{
public:

	typedef	std::vector<std::string>	Vector;

	std::string	TargetName;
	bfs::path	ProjectPath;
	bfs::path	ProjectBuildPath;
	bfs::path	IncludeBuildPath;
	bfs::path	IncludeRootPath;
	bfs::path	SrcBuildPath;
	bfs::path	SrcRootPath;
	Vector		AdditionalIncludeDirectories;
	Vector		AdditionalDependencies;
	Vector		AdditionalLibraryDirectories;
};

boost::optional<SProjectInfo>	ReadConfig();
bool			BuildProject(const SProjectInfo& projInfo);