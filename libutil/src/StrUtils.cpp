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

std::string StrUtils::trim(std::string &string, const char *trimCharacterList)
{
    // 左側からトリムする文字以外が見つかる位置を検索します。
    std::string::size_type left = string.find_first_not_of(trimCharacterList);

    if (left != std::string::npos)
    {
        // 左側からトリムする文字以外が見つかった場合は、同じように右側からも検索します。
        std::string::size_type right = string.find_last_not_of(trimCharacterList);

        // 戻り値を決定します。ここでは右側から検索しても、トリムする文字以外が必ず存在するので判定不要です。
        string = string.substr(left, right - left + 1);
    }

    return string;
}

std::string StrUtils::trim(const std::string &string, const char *trimCharacterList)
{
    std::string result;

    // 左側からトリムする文字以外が見つかる位置を検索します。
    std::string::size_type left = string.find_first_not_of(trimCharacterList);

    if (left != std::string::npos)
    {
        // 左側からトリムする文字以外が見つかった場合は、同じように右側からも検索します。
        std::string::size_type right = string.find_last_not_of(trimCharacterList);

        // 戻り値を決定します。ここでは右側から検索しても、トリムする文字以外が必ず存在するので判定不要です。
        result = string.substr(left, right - left + 1);
    }

    return result;
}

std::string StrUtils::ltrim(std::string &string, const char *trimCharacterList)
{
    // 左側からトリムする文字以外が見つかる位置を検索します。
    std::string::size_type left = string.find_first_not_of(trimCharacterList);

    if (left != std::string::npos)
    {
        // 戻り値を決定します。ここでは右側から検索しても、トリムする文字以外が必ず存在するので判定不要です。
        string = string.substr(left);
    }
    else {
        string = "";
    }

    return string;
}

std::string StrUtils::ltrim(const std::string &string, const char *trimCharacterList)
{
    std::string result;

    // 左側からトリムする文字以外が見つかる位置を検索します。
    std::string::size_type left = string.find_first_not_of(trimCharacterList);

    if (left != std::string::npos)
    {
        // 戻り値を決定します。ここでは右側から検索しても、トリムする文字以外が必ず存在するので判定不要です。
        result = string.substr(left);
    }
    else {
        result = "";
    }

    return result;
}