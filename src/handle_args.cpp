/*
 *	Copyright © 2016 tastytea <tastytea@tastytea.de>
 *
 *	This file is part of seriespl.
 *
 *	seriespl is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, version 2 of the License.
 *
 *	seriespl is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along
 *	with seriespl.  If not, see <https://www.gnu.org/licenses/gpl-2.0.html>.
 *
 ******************************************************************************/


#include "seriespl.hpp"
#include <iostream>
#include <vector>
#include <getopt.h>
#include <unistd.h>	// sleep()
#include <cstring>	// strncmp()

int Seriespl::handle_args(int argc, char const *argv[])
{
	int opt;
	std::string usage = std::string("usage: ") + argv[0] +
		" [-h] [-i]|[-p list] [-e episodes] [-s seasons] [-f format] [-y] URL";
	
	while ((opt = getopt(argc, (char **)argv, "hp:ie:s:f:yV")) != -1)
	{
		std::string episodes, seasons;
		size_t pos;
		switch (opt)
		{
			case 'h':	// Help
				std::cout << usage << "\n\n"
				"  -h                   Show this help\n"
				"  -p stream providers  Comma delimited list. Available:\n"
				"                       Streamcloud,Vivo,Shared,YouTube,OpenLoad,OpenLoadHD,"
				                        "PowerWatch,CloudTime,AuroraVid,Vidto\n"
				"  -i                   Use stream providers without SSL support too\n"
				"  -e                   Episode range, e.g. 2-5 or 7 or 9-, use c for current\n"
				"  -s                   Season range, e.g. 1-2 or 4\n"
				"  -f                   Playlist format. Available: raw, m3u, pls\n"
				"  -y                   Use youtube-dl to print the direct URL of the video file\n"
				"  -V                   Output version and copyright information and exit" << std::endl;
				return -1;
				break;
			case 'p':	// Provider
				populate_providers(std::string(optarg));
				break;
			case 'i':	// Insecure
				// Add Providers without SSL support
				Providers.insert(Providers.end(),
					Providers_nossl.begin(), Providers_nossl.end());
				break;
			case 'e':	// Episodes
				episodes = optarg;
				pos = episodes.find('-');
				if (pos != std::string::npos)
				{
					try
					{
						if (episodes.substr(0, pos) == "c")
						{
							current_episode |= 1;
						}
						else
						{
							startEpisode = std::stoi( episodes.substr(0, pos) );
						}
						if (episodes.length() > pos + 1)
						{ // If episodes = 5-, output all episodes, beginning with 5
							if (episodes.substr(pos + 1) == "c")
							{
								current_episode |= 2;
							}
							else
							{
								endEpisode = std::stoi( episodes.substr(pos + 1) );
							}
						}
					}
					catch (std::exception &e)
					{ // There is a '-' but no numbers around it
						std::cerr << "Error: Can not decipher episode range, " <<
							"defaulting to all." << std::endl;
						sleep(2);
					}
				}
				else
				{
					try
					{ 
						if (episodes == "c")
						{
							current_episode = 3;
						}
						else
						{ // Is episodes a single number?
							startEpisode = std::stoi(episodes);
							endEpisode = std::stoi(episodes);
						}
					}
					catch (std::exception &e)
					{
						std::cerr << "Error: Can not decipher episode range, " <<
							"defaulting to all." << std::endl;
						sleep(2);
					}
				}
				break;
			case 's':	// Seasons
				seasons = optarg;
				pos = seasons.find('-');
				if (pos != std::string::npos)
				{
					try
					{
						startSeason = std::stoi( seasons.substr(0, pos) );
						endSeason = std::stoi( seasons.substr(pos + 1) );
					}
					catch (std::exception &e)
					{ // There is a '-' but no numbers around it
						std::cerr << "Error: Can not decipher season range, " <<
							"defaulting to selected." << std::endl;
						startSeason = -1;
						endSeason = -1;
						sleep(2);
					}
				}
				else
				{
					try
					{ // Is seasons a single number?
						startSeason = std::stoi(seasons);
						endSeason = std::stoi(seasons);
					}
					catch (std::exception &e)
					{
						std::cerr << "Error: Can not decipher season range, " <<
							"defaulting to selected." << std::endl;
						sleep(2);
					}
				}
				break;
			case 'f':	// Format
				if (strncmp(optarg, "raw", 3) == 0)
					playlist = PL_RAW;
				else if (strncmp(optarg, "m3u", 3) == 0)
					playlist = PL_M3U;
				else if (strncmp(optarg, "pls", 3) == 0)
					playlist = PL_PLS;
				else
					std::cerr << "Playlist format not recognized, defaulting to raw." << std::endl;
				break;
			case 'y':	// youtube-dl
				direct_url = true;
				break;
			case 'V':	// Version
				std::cout << "seriespl " << version << "\n"
						  << "Copyright © 2016 tastytea <tastytea@tastytea.de>.\n"
						  << "License GPLv2: GNU GPL version 2 <https://www.gnu.org/licenses/gpl-2.0.html>.\n"
						  << "This is free software: you are free to change and redistribute it.\n"
						  << "There is NO WARRANTY, to the extent permitted by law." << std::endl;
				return -1;
				break;
			default:
				std::cerr << usage << std::endl;
				return 1;
				break;
		}
	}
	if (optind >= argc)
	{
		std::cerr << usage << std::endl;
		return 1;
	}
	else
	{ // Set URL of tv series
		directoryurl = argv[optind];
	}

	return 0;
}
