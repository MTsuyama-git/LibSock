#include <StrUtils.hpp>

std::vector<std::string> StrUtils::split(const std::string &str, const std::string &del)
{
    int first = str.find_first_not_of(del);
    int last = str.find_first_of(del, first);

    std::vector<std::string> result;

    while (first < str.size())
    {
        std::string subStr(str, first, last - first);

        result.push_back(subStr);

        first = str.find_first_not_of(del, last);
        last = str.find_first_of(del, first);

        if (last == std::string::npos)
        {
            last = str.size();
        }
    }

    return result;
}