#include "ProjConvertor.h"

#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range.hpp>

#include <iostream>


boost::optional<SProjectInfo> ReadConfig()
{
	SProjectInfo projInfo;

	if ( !bfs::exists("config.xml") )
	{
		std::cerr << "Need Config File." << std::endl;
		return boost::none;
	}

	boost::filesystem::ifstream configIfs("config.xml");
	boost::property_tree::ptree configXml;

	try
	{
		boost::property_tree::read_xml(configIfs, configXml, xml_parser::no_comments | xml_parser::trim_whitespace);
	}
	catch ( std::exception& exp )
	{
		std::cerr << exp.what() << std::endl;
		return boost::none;
	}

	auto targetName = configXml.get_optional<std::string>("Project.TargetName.<xmlattr>.Name");
	if ( !targetName )
	{
		std::cerr << "Need Target Name." << std::endl;
		return boost::none;
	}
	projInfo.TargetName = *targetName;

	auto projPath = configXml.get_optional<std::string>("Project.ProjectPath.<xmlattr>.Path");
	if ( !projPath )
	{
		std::cerr << "Need Project Path." << std::endl;
		return boost::none;
	}
	projInfo.ProjectPath = bfs::system_complete(*projPath);

	auto projBuildPath = configXml.get_optional<std::string>("Project.ProjectBuildPath.<xmlattr>.Path");
	if ( !projBuildPath )
	{
		std::cerr << "Need Project Build Path." << std::endl;
		return boost::none;
	}
	projInfo.ProjectBuildPath = bfs::system_complete(*projBuildPath);

	auto includePath = configXml.get_optional<std::string>("Project.IncludePath.<xmlattr>.Path");
	if ( !includePath )
	{
		std::cerr << "Need Include Path." << std::endl;
		return boost::none;
	}
	projInfo.IncludePath = bfs::system_complete(*includePath);

	auto includeCopyToPath = configXml.get_optional<std::string>("Project.IncludeCopyTo.<xmlattr>.Path");
	if ( !includeCopyToPath )
	{
		std::cerr << "Need Include Copy To Path." << std::endl;
		return boost::none;
	}
	projInfo.IncludeCopyTo = bfs::system_complete(*includeCopyToPath);

	auto srcPath = configXml.get_optional<std::string>("Project.SrcPath.<xmlattr>.Path");
	if ( !srcPath )
	{
		std::cerr << "Need Src Path." << std::endl;
		return boost::none;
	}
	projInfo.SrcPath = bfs::system_complete(*srcPath);

	auto srcCopyToPath = configXml.get_optional<std::string>("Project.SrcCopyTo.<xmlattr>.Path");
	if ( !srcCopyToPath )
	{
		std::cerr << "Need Src Copy To Path." << std::endl;
		return boost::none;
	}
	projInfo.SrcCopyTo = bfs::system_complete(*srcCopyToPath);

	auto additionalIncDirs = configXml.get_child_optional("Project.AdditionalIncludeDirectories");
	if ( additionalIncDirs )
	{
		for ( auto& curAdditionalIncDir : *additionalIncDirs )
		{
			auto item = curAdditionalIncDir.second.get_optional<std::string>("<xmlattr>.Path");
			if ( item )
			{
				projInfo.AdditionalIncludeDirectories.push_back(*item);
			}
		}
	}

	auto additionalDepends = configXml.get_child_optional("Project.AdditionalDependencies");
	if ( additionalDepends )
	{
		for ( auto& curAdditionalDep : *additionalDepends )
		{
			auto item = curAdditionalDep.second.get_optional<std::string>("<xmlattr>.Path");
			if ( item )
			{
				projInfo.AdditionalDependencies.push_back(*item);
			}
		}
	}

	auto additionalLibsDirs = configXml.get_child_optional("Project.AdditionalLibraryDirectories");
	if ( additionalLibsDirs )
	{
		for ( auto& curAdditionalLibDir : *additionalLibsDirs )
		{
			auto item = curAdditionalLibDir.second.get_optional<std::string>("<xmlattr>.Path");
			if ( item )
			{
				projInfo.AdditionalLibraryDirectories.push_back(*item);
			}
		}
	}

	return projInfo;
}

bool CheckConfig(const SProjectInfo& projInfo)
{
	auto projPath = projInfo.ProjectPath / (projInfo.TargetName + ".vcxproj");
	if ( !bfs::exists(projPath) )
	{
		std::cerr << "Can not Find " << bfs::absolute(projPath) << std::endl;
		return false;
	}

	auto filterPath = projInfo.ProjectPath / (projInfo.TargetName + ".vcxproj.filters");
	if ( !bfs::exists(filterPath) )
	{
		std::cerr << "Can not Find " << bfs::absolute(filterPath) << std::endl;
		return false;
	}

	if ( !bfs::exists(projInfo.IncludePath) )
	{
		std::cerr << "Can not Find " << bfs::absolute(projInfo.IncludePath) << std::endl;
		return false;
	}

	if ( !bfs::exists(projInfo.SrcPath) )
	{
		std::cerr << "Can not Find " << bfs::absolute(projInfo.SrcPath) << std::endl;
		return false;
	}

	return true;
}

bfs::path RelativeTo(bfs::path from, bfs::path to)
{
	// Start at the root path and while they are the same then do nothing then when they first
	// diverge take the remainder of the two path and replace the entire from path with ".."
	// segments.
	auto fromIter = from.begin();
	auto toIter = to.begin();

	// Loop through both
	while ( fromIter != from.end() && toIter != to.end() && (*toIter) == (*fromIter) )
	{
		++toIter;
		++fromIter;
	}

	bfs::path finalPath;
	while ( fromIter != from.end() )
	{
		finalPath /= "..";
		++fromIter;
	}

	while ( toIter != to.end() )
	{
		if ( *toIter != "." )
		{
			finalPath /= *toIter;
		}
		
		++toIter;
	}

	return finalPath;
}


bool	BuildVCXPROJ(const SProjectInfo& projInfo)
{
	auto projFileName = projInfo.ProjectPath / (projInfo.TargetName + ".vcxproj");

	ptree rawProjXml, outputProjXml;

	try
	{
		boost::filesystem::fstream projIfs(projFileName);
		boost::property_tree::read_xml(projIfs, rawProjXml, xml_parser::no_comments | xml_parser::trim_whitespace);
	}
	catch ( std::exception& exp )
	{
		std::cerr << exp.what() << std::endl;
		return false;
	}

	auto projItems = rawProjXml.get_child_optional("Project");
	if ( !projItems )
	{
		std::cerr << "Can not find <Project>." << std::endl;
		return false;
	}

	auto relProjToIncludePath = RelativeTo(projInfo.ProjectBuildPath, projInfo.IncludeCopyTo);
	auto relProjToSrcPath = RelativeTo(projInfo.ProjectBuildPath, projInfo.SrcCopyTo);

	{//Copy
		if ( bfs::exists(projInfo.IncludeCopyTo) )
		{
			bfs::remove_all(projInfo.IncludeCopyTo);
		}
		bfs::create_directories(projInfo.IncludeCopyTo);
		
		bfs::recursive_directory_iterator itor(projInfo.IncludePath), itorEnd;
		for ( auto& curFile : boost::make_iterator_range( itor, itorEnd) )
		{
			auto relPath = RelativeTo(projInfo.IncludePath, curFile);

			if ( bfs::is_directory(curFile) )
			{
				bfs::create_directories(projInfo.IncludeCopyTo / relPath);
			}
			else
			{
				bfs::copy_file(curFile, projInfo.IncludeCopyTo / relPath, bfs::copy_option::overwrite_if_exists);
			}
		}
	}
	{
		if ( bfs::exists(projInfo.SrcCopyTo) )
		{
			bfs::remove_all(projInfo.SrcCopyTo);
		}
		bfs::create_directories(projInfo.SrcCopyTo);
		bfs::recursive_directory_iterator itor(projInfo.SrcPath), itorEnd;
		for ( auto& curFile : boost::make_iterator_range(itor, itorEnd) )
		{
			auto relPath = RelativeTo(projInfo.SrcPath, curFile);

			if ( bfs::is_directory(curFile) )
			{
				bfs::create_directories(projInfo.SrcCopyTo / relPath);
			}
			else
			{
				bfs::copy_file(curFile, projInfo.SrcCopyTo / relPath, bfs::copy_option::overwrite_if_exists);
			}
		}
	}

	for ( auto& curProjItem : *projItems )
	{
		if ( curProjItem.first == "PropertyGroup" )
		{
			auto outDir = curProjItem.second.get_optional<std::string>("OutDir");
			if ( !outDir )
			{
				outputProjXml.add_child("Project." + curProjItem.first, curProjItem.second);
				continue;
			}

			ptree tmpPG;
			for ( auto& curPropertyGroupItem : curProjItem.second )
			{
				if ( curPropertyGroupItem.first == "OutDir" )
				{
					auto dir = curPropertyGroupItem.second.get_value<std::string>();
					ptree outDir;
					outDir.add_child("<xmlattr>", curPropertyGroupItem.second.get_child("<xmlattr>"));
					outDir.put_value(R"($(SolutionDir)build\bin\$(Configuration)\$(Platform)\$(PlatformToolset)\)");

					tmpPG.add_child(curPropertyGroupItem.first, outDir);
				}
				else if ( curPropertyGroupItem.first == "IntDir" )
				{
					auto dir = curPropertyGroupItem.second.get_value<std::string>();
					ptree intDir;
					intDir.add_child("<xmlattr>", curPropertyGroupItem.second.get_child("<xmlattr>"));
					intDir.put_value(R"($(SolutionDir)build\obj\$(ProjectName)\$(Configuration)\$(Platform)\$(PlatformToolset)\)");

					tmpPG.add_child(curPropertyGroupItem.first, intDir);
				}
				else if ( curPropertyGroupItem.first != "TargetName" )
				{
					tmpPG.add_child(curPropertyGroupItem.first, curPropertyGroupItem.second);
				}
			}
			outputProjXml.add_child("Project.PropertyGroup", tmpPG);
		}
		else if ( curProjItem.first == "ItemDefinitionGroup" )
		{
			ptree tmpIDG;
			//tmpIDG.add_child("<xmlattr>", curProjItem.second.get_child("<xmlattr>"));

			static std::regex rg(R"((.+?)(;|$))");

			for ( auto& curIDGItem : curProjItem.second )
			{
				//curIDGItem.first will be "ClCompile"/"ResourceCompile"/"Midl"/"Link"/"ProjectReference"

				for ( auto& curID : curIDGItem.second )
				{
					if ( curID.first == "AdditionalIncludeDirectories" )
					{
						std::string finalStr;
						for ( auto& curStr : projInfo.AdditionalIncludeDirectories )
						{
							finalStr += curStr + ";";
						}
						finalStr += "%(AdditionalIncludeDirectories)";

						tmpIDG.add(curIDGItem.first + "." + curID.first, finalStr);
					}
					else if ( curID.first == "PreprocessorDefinitions" )
					{
						auto rawStr = curID.second.get_value<std::string>();

						std::sregex_iterator itor(rawStr.begin(), rawStr.end(), rg), itorEnd;

						std::string finalStr;
						for ( auto& curStr : boost::make_iterator_range(itor, itorEnd) )
						{
							auto predef = curStr.str(1);
							if ( predef != "BOOST_ALL_NO_LIB" )
							{
								finalStr += curStr.str(1) + ";";
							}
						}

						finalStr.erase(finalStr.size() - 1);

						tmpIDG.add(curIDGItem.first + "." + curID.first, finalStr);
					}
					else if ( curID.first == "AdditionalDependencies" )
					{
						std::string finalStr;
						for ( auto& curStr : projInfo.AdditionalDependencies )
						{
							finalStr += curStr + ";";
						}
						finalStr += "%(AdditionalDependencies)";

						tmpIDG.add(curIDGItem.first + "." + curID.first, finalStr);
					}
					else if ( curID.first == "AdditionalLibraryDirectories" )
					{
						std::string finalStr;
						for ( auto& curStr : projInfo.AdditionalLibraryDirectories )
						{
							finalStr += curStr + ";";
						}
						finalStr += "%(AdditionalLibraryDirectories)";

						tmpIDG.add(curIDGItem.first + "." + curID.first, finalStr);
					}
					else if ( curID.first == "ImportLibrary" )
					{
						tmpIDG.add(curIDGItem.first + "." + curID.first, R"($(SolutionDir)build\lib\$(ProjectName)\$(Configuration)\$(Platform)\$(PlatformToolset)\$(TargetName).lib)");
					}
					else if ( curID.first != "AssemblerListingLocation" )
					{
						tmpIDG.add_child(curIDGItem.first + "." + curID.first, curID.second);
					}
				}

				if ( curIDGItem.first == "ClCompile" )
				{
					tmpIDG.put(curIDGItem.first + ".ProgramDataBaseFileName", R"($(IntDir)vc$(PlatformToolsetVersion)$(TargetName).pdb)");
				}
			}

			outputProjXml.add_child("Project.ItemDefinitionGroup", tmpIDG);
		}
		else if ( curProjItem.first == "ItemGroup" )
		{
			ptree tmpIG;

			for ( auto& curItemGroup : curProjItem.second )
			{
				if ( curItemGroup.first == "ClInclude" )//TODO:H�ļ�
				{
					ptree tmpFile;

					auto hFile = curItemGroup.second.get<std::string>("<xmlattr>.Include");

					auto filePath = bfs::system_complete(projInfo.ProjectPath / hFile);
					auto includeRelPath = RelativeTo(projInfo.IncludePath, filePath);
					auto includePath = relProjToIncludePath / includeRelPath;
					auto srcRelPath = RelativeTo(projInfo.SrcPath, filePath);
					auto srcPath = relProjToSrcPath / srcRelPath;

					if ( bfs::exists(projInfo.ProjectBuildPath / includePath) )
					{
						tmpFile.add("<xmlattr>.Include", (relProjToIncludePath / includeRelPath).string());
					}
					else if ( bfs::exists(projInfo.ProjectBuildPath / srcPath) )
					{
						tmpFile.add("<xmlattr>.Include", (relProjToSrcPath / srcRelPath).string());
					}
					else
					{
						auto curRel = RelativeTo(projInfo.ProjectBuildPath, projInfo.IncludeCopyTo);
						bfs::copy_file(filePath, projInfo.IncludeCopyTo / filePath.filename(), bfs::copy_option::overwrite_if_exists);
						tmpFile.add("<xmlattr>.Include", (curRel / filePath.filename()).string());
					}
					

					tmpIG.add_child(curItemGroup.first, tmpFile);
				}
				else if ( curItemGroup.first == "ClCompile" || curItemGroup.first == "ResourceCompile" )//TODO:CPP�ļ�
				{
					ptree tmpFile;

					for ( auto& cppItem : curItemGroup.second )
					{
						if ( cppItem.first == "<xmlattr>" )
						{
							auto cppFile = curItemGroup.second.get<std::string>("<xmlattr>.Include");

							auto filePath = bfs::system_complete(projInfo.ProjectPath / cppFile);
							auto relPath = RelativeTo(projInfo.SrcPath, filePath);


							tmpFile.add("<xmlattr>.Include", (relProjToSrcPath / relPath).string());
						}
						else
						{
							tmpFile.add_child(cppItem.first, cppItem.second);
						}
					}

					tmpIG.add_child(curItemGroup.first, tmpFile);
				}
				else
				{
					tmpIG.add_child(curItemGroup.first, curItemGroup.second);
				}
			}

			outputProjXml.add_child("Project." + curProjItem.first, tmpIG);
		}
		else
		{
			outputProjXml.add_child("Project." + curProjItem.first, curProjItem.second);
		}

	}
	
	{
		bfs::create_directories(projInfo.ProjectBuildPath);

		boost::filesystem::fstream ofs(projInfo.ProjectBuildPath / (projInfo.TargetName + ".vcxproj"), std::ios::trunc | std::ios::out);
		auto settings = xml_writer_settings<std::string>();
		settings.indent_count = 2;
		//settings.indent_char = '\t';
		write_xml(ofs, outputProjXml, settings);
	}

	return true;
}

bool	BuildFilter(const SProjectInfo& projInfo)
{
	auto filterFileName = projInfo.ProjectPath / (projInfo.TargetName + ".vcxproj.filters");

	ptree rawFilterXml, outputFilterXml;

	try
	{
		boost::filesystem::fstream filterIfs(filterFileName);
		boost::property_tree::read_xml(filterIfs, rawFilterXml, xml_parser::no_comments | xml_parser::trim_whitespace);
	}
	catch ( std::exception& exp )
	{
		std::cerr << exp.what() << std::endl;
		return false;
	}

	auto projItems = rawFilterXml.get_child_optional("Project");
	if ( !projItems )
	{
		std::cerr << "Can not find <Project>." << std::endl;
		return false;
	}

	auto relProjToIncludePath = RelativeTo(projInfo.ProjectBuildPath, projInfo.IncludeCopyTo);
	auto relProjToSrcPath = RelativeTo(projInfo.ProjectBuildPath, projInfo.SrcCopyTo);

	for ( auto& curProjItem : *projItems )
	{
		if ( curProjItem.first == "ItemGroup" )
		{
			ptree tmpIG;

			for ( auto& curItem : curProjItem.second )
			{
				if ( curItem.first == "Filter" )
				{
					tmpIG.add_child(curItem.first, curItem.second);
				}
				else
				{
					ptree item; //Compile

					for ( auto& curItemProperty : curItem.second )
					{
						if ( curItemProperty.first == "<xmlattr>" )
						{
							bfs::path file = curItemProperty.second.get<std::string>("Include");

							auto filePath = bfs::system_complete(projInfo.ProjectPath / file);
							auto includeRelPath = RelativeTo(projInfo.IncludePath, filePath);
							auto includePath = relProjToIncludePath / includeRelPath;
							auto srcRelPath = RelativeTo(projInfo.SrcPath, filePath);
							auto srcPath = relProjToSrcPath / srcRelPath;

							if ( bfs::exists(projInfo.ProjectBuildPath / includePath) )
							{
								item.add("<xmlattr>.Include", (relProjToIncludePath / includeRelPath).string());
							}
							else if ( bfs::exists(projInfo.ProjectBuildPath / srcPath) )
							{
								item.add("<xmlattr>.Include", (relProjToSrcPath / srcRelPath).string());
							}
							else if ( bfs::exists(projInfo.IncludeCopyTo/filePath.filename()))
							{
								item.add("<xmlattr>.Include", (relProjToIncludePath / filePath.filename()).string());
							}
							else
							{
								assert(0);
							}
						}
						else
						{
							item.add_child(curItemProperty.first, curItemProperty.second);
						}
					}

					tmpIG.add_child(curItem.first, item);
				}
			}

			outputFilterXml.add_child("Project." + curProjItem.first, tmpIG);
		}
		else
		{
			outputFilterXml.add_child("Project." + curProjItem.first, curProjItem.second);
		}
	}

	{
		bfs::create_directories(projInfo.ProjectBuildPath);

		boost::filesystem::fstream ofs(projInfo.ProjectBuildPath / (projInfo.TargetName + ".vcxproj.filters"), std::ios::trunc | std::ios::out);
		auto settings = xml_writer_settings<std::string>();
		settings.indent_count = 2;
		//settings.indent_char = '\t';
		write_xml(ofs, outputFilterXml, settings);
	}

	return true;
}

bool BuildProject(const SProjectInfo& projInfo)
{
	if ( !CheckConfig(projInfo) )
	{
		return false;
	}

 	if ( !BuildVCXPROJ(projInfo) )
 	{
 		return false;
 	}

	if ( !BuildFilter(projInfo) )
	{
		return false;
	}

	return true;
}
