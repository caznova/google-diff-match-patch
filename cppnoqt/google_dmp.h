#pragma once

enum class Operation {
	DELETE, INSERT, EQUAL
};

class Diff
{
public:
	Diff()
	{

	}

	Diff(Operation oper, std::string & str)
	{
		operation = oper;
		text = str;
	}

	inline bool operator==(Diff & rhs)
	{
		return rhs.operation == operation && rhs.text == text;
	}

	Operation operation;
	std::string text;
};

class google_dmp
{
private:
	// Defaults.
	// Set these on your diff_match_patch instance to override the defaults.
	// Number of seconds to map a diff before giving up (0 for infinity).
	float Diff_Timeout = 1.0f;
	// Cost of an empty edit operation in terms of edit characters.
	short Diff_EditCost = 4;
	// At what point is no match declared (0.0 = perfection, 1.0 = very loose).
	float Match_Threshold = 0.5f;
	// How far to search for a match (0 = exact location, 1000+ = broad match).
	// A match this many characters away from the expected location will add
	// 1.0 to the score (0.0 is a perfect match).
	int Match_Distance = 1000;
	// When deleting a large block of text (over ~64 characters), how close
	// do the contents have to be to match the expected contents. (0.0 =
	// perfection, 1.0 = very loose).  Note that Match_Threshold controls
	// how closely the end points of a delete need to match.
	float Patch_DeleteThreshold = 0.5f;
	// Chunk size for context length.
	short Patch_Margin = 4;
	// The number of bits in an int.
	short Match_MaxBits = 32;

public:

	template<class T>
	std::vector<T> * SpliceIL(std::vector<T> * input, int start, int count, std::initializer_list<T> objects = {})
	{
		while (count > 0)
		{
			input->erase(input->begin() + start);
			count--;
		}
		for (auto i : objects)
		{
			input->insert(input->begin() + start++, i);
		}

		return nullptr;
	}

	bool isBlankLine(char const* line)
	{
		for (char const* cp = line; *cp; ++cp)
		{
			if (!isspace(*cp)) return false;
		}
		return true;
	}

	bool isBlankLine(std::string const& line)
	{
		return isBlankLine(line.c_str());
	}


	std::vector<Diff*> * diff_main(std::string text1, std::string text2, bool checklines = true);
	int diff_commonPrefix(std::string text1, std::string text2);
	int diff_commonSuffix(std::string text1, std::string text2);
	std::vector<Diff*> * diff_compute(std::string text1, std::string text2, bool checklines = true);
	std::vector<std::string> * diff_halfMatch(std::string text1, std::string text2);
	std::vector<std::string> * diff_halfMatchI(std::string longtext, std::string shorttext, int i);
	std::vector<Diff*> *  diff_lineMode(std::string text1, std::string text2);
	std::tuple<std::string, std::string, std::vector<std::string>> diff_linesToChars(std::string text1, std::string text2);
	std::string diff_linesToCharsMunge(std::string text, std::vector<std::string> & lineArray, std::unordered_map<std::string, int> & lineHash);
	void diff_charsToLines(std::vector<Diff*> * diffs, std::vector<std::string> & lineArray);
	void diff_cleanupSemantic(std::vector<Diff*> * diffs);
	void diff_cleanupMerge(std::vector<Diff*> * diffs);
	void diff_cleanupSemanticLossless(std::vector<Diff*> * diffs);
	int diff_cleanupSemanticScore(std::string one, std::string two);
	int diff_commonOverlap(std::string text1, std::string text2);
	std::vector<Diff*> *  diff_bisect(std::string text1, std::string text2, std::time_t deadline);
	std::vector<Diff*> *  diff_bisectSplit(std::string text1, std::string text2, int x, int y, std::time_t deadline);
};