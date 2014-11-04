#include "stdafx.h"

std::vector<Diff*> * google_dmp::diff_main(std::string text1, std::string text2, bool checklines)
{
	std::vector<Diff*> * diffs;
	if (text1 == text2) 
	{
		diffs = new std::vector<Diff*>();
		if (text1.length() != 0) 
		{
			diffs->push_back(new Diff(Operation::EQUAL, text1));
		}
		return diffs;
	}

	// Trim off common prefix (speedup).
	int commonlength = diff_commonPrefix(text1, text2);
	std::string commonprefix = text1.substr(0, commonlength);
	text1 = text1.substr(commonlength);
	text2 = text2.substr(commonlength);

	// Trim off common suffix (speedup).
	commonlength = diff_commonSuffix(text1, text2);
	std::string commonsuffix = text1.substr(text1.length() - commonlength);
	text1 = text1.substr(0, text1.length() - commonlength);
	text2 = text2.substr(0, text2.length() - commonlength);

	// Compute the diff on the middle block.
	diffs = diff_compute(text1, text2 , checklines);

	// Restore the prefix and suffix.
	if (commonprefix.length() != 0) {
		diffs->insert(diffs->begin(), (new Diff(Operation::EQUAL, commonprefix)));
	}
	if (commonsuffix.length() != 0) {
		diffs->push_back(new Diff(Operation::EQUAL, commonsuffix));
	}

	diff_cleanupMerge(diffs);
	return diffs;
}

int google_dmp::diff_commonPrefix(std::string text1, std::string text2)
{
	int n = std::min(text1.length(), text2.length());
	for (int i = 0; i < n; i++)
	{
		if (text1[i] != text2[i])
		{
			return i;
		}
	}
	return n;
}

int google_dmp::diff_commonSuffix(std::string text1, std::string text2)
{
	int text1_length = text1.length();
	int text2_length = text2.length();
	int n = std::min(text1.length(), text2.length());
	for (int i = 1; i <= n; i++)
	{
		if (text1[text1_length - i] != text2[text2_length - i])
		{
			return i - 1;
		}
	}
	return n;
}

std::vector<Diff*> * google_dmp::diff_compute(std::string text1, std::string text2, bool checklines /*= true*/)
{
	std::vector<Diff*> * diffs = new std::vector<Diff*>();

	if (text1.length() == 0)
	{
		// Just add some text (speedup).
		diffs->push_back(new Diff(Operation::INSERT, text2));
		return diffs;
	}

	if (text2.length() == 0)
	{
		// Just delete some text (speedup).
		diffs->push_back(new Diff(Operation::DELETE, text1));
		return diffs;
	}

	std::string longtext = text1.length() > text2.length() ? text1 : text2;
	std::string shorttext = text1.length() > text2.length() ? text2 : text1;
	int i = longtext.find(shorttext);
	if (i != -1)
	{
		// Shorter text is inside the longer text (speedup).
		Operation op = (text1.length() > text2.length()) ? Operation::DELETE : Operation::INSERT;
		diffs->push_back(new Diff(op, longtext.substr(0, i)));
		diffs->push_back(new Diff(Operation::EQUAL, shorttext));
		diffs->push_back(new Diff(op, longtext.substr(i + shorttext.length())));
		return diffs;
	}

	if (shorttext.length() == 1) {
		// Single character string.
		// After the previous speedup, the character can't be an equality.
		diffs->push_back(new Diff(Operation::DELETE, text1));
		diffs->push_back(new Diff(Operation::INSERT, text2));
		return diffs;
	}

	// Check to see if the problem can be split in two.
	std::vector<std::string> * hm = diff_halfMatch(text1, text2);
	if (hm != nullptr)
	{
		// A half-match was found, sort out the return data.
		std::string text1_a = (*hm)[0];
		std::string text1_b = (*hm)[1];
		std::string text2_a = (*hm)[2];
		std::string text2_b = (*hm)[3];
		std::string mid_common = (*hm)[4];
		// Send both pairs off for separate processing.
		std::vector<Diff*> * diffs_a = diff_main(text1_a, text2_a);
		std::vector<Diff*> * diffs_b = diff_main(text1_b, text2_b);
		if (diffs_a == nullptr){ diffs_a = new std::vector<Diff*>(); }
		// Merge the results.
		diffs = diffs_a;
		diffs->push_back(new Diff(Operation::EQUAL, mid_common));
		std::copy(diffs_b->begin(), diffs_b->end(), std::back_inserter(*diffs));
		return diffs;
	}

	if (checklines && text1.length() > 100 && text2.length() > 100) {
		return diff_lineMode(text1, text2);
	}

	return diff_bisect(text1, text2, std::time(nullptr) + 600);
}

std::vector<std::string> * google_dmp::diff_halfMatch(std::string text1, std::string text2)
{
	std::string longtext = text1.length() > text2.length() ? text1 : text2;
	std::string shorttext = text1.length() > text2.length() ? text2 : text1;
	if (longtext.length() < 4 || shorttext.length() * 2 < longtext.length())
	{
		return nullptr;  // Pointless.
	}

	// First check if the second quarter is the seed for a half-match.
	std::vector<std::string> * hm1 = diff_halfMatchI(longtext, shorttext, (longtext.length() + 3) / 4);
	// Check again based on the third quarter.
	std::vector<std::string> * hm2 = diff_halfMatchI(longtext, shorttext, (longtext.length() + 1) / 2);
	std::vector<std::string> * hm = new std::vector<std::string>();
	hm->reserve(5);
	if (hm1 == nullptr && hm2 == nullptr)
	{
		return nullptr;
	}
	else if (hm2 == nullptr)
	{
		hm = hm1;
	}
	else if (hm1 == nullptr)
	{
		hm = hm2;
	}
	else
	{
		// Both matched.  Select the longest.
		hm = (*hm1)[4].length() > (*hm2)[4].length() ? hm1 : hm2;
	}

	// A half-match was found, sort out the return data.
	if (text1.length() > text2.length())
	{
		return hm;
		//return new string[]{hm[0], hm[1], hm[2], hm[3], hm[4]};
	}
	else
	{
		std::swap((*hm1)[2], (*hm1)[0]); //2 1 0 3 4  
		std::swap((*hm1)[3], (*hm1)[1]); //2 3 0 1 4  
		return hm1;
	}
}

std::vector<std::string> * google_dmp::diff_halfMatchI(std::string longtext, std::string shorttext, int i)
{
	// Start with a 1/4 length Substring at position i as a seed.
	std::string seed = longtext.substr(i, longtext.length() / 4);
	int j = -1;
	std::string best_common = "";
	std::string best_longtext_a = "", best_longtext_b = "";
	std::string best_shorttext_a = "", best_shorttext_b = "";
	while (j < shorttext.length() && (j = shorttext.find(seed, j + 1)) != -1)
	{
		int prefixLength = diff_commonPrefix(longtext.substr(i),
			shorttext.substr(j));
		int suffixLength = diff_commonSuffix(longtext.substr(0, i),
			shorttext.substr(0, j));
		if (best_common.length() < suffixLength + prefixLength)
		{
			best_common = shorttext.substr(j - suffixLength, suffixLength)
				+ shorttext.substr(j, prefixLength);
			best_longtext_a = longtext.substr(0, i - suffixLength);
			best_longtext_b = longtext.substr(i + prefixLength);
			best_shorttext_a = shorttext.substr(0, j - suffixLength);
			best_shorttext_b = shorttext.substr(j + prefixLength);
		}
	}
	if (best_common.length() * 2 >= longtext.length())
	{
		std::vector<std::string> * hm = new std::vector<std::string>();
		hm->push_back(best_longtext_a);
		hm->push_back(best_longtext_b);
		hm->push_back(best_shorttext_a);
		hm->push_back(best_shorttext_b);
		hm->push_back(best_common);
		return hm;
	}
	else
	{
		return nullptr;
	}
}

std::vector<Diff*> * google_dmp::diff_lineMode(std::string text1, std::string text2)
{
	// Scan the text on a line-by-line basis first.
	std::tuple<std::string, std::string, std::vector<std::string>> b = diff_linesToChars(text1, text2);
	text1 = (std::string)std::get<0>(b);
	text2 = (std::string)std::get<1>(b);
	std::vector<std::string> linearray = std::get<2>(b);

	std::vector<Diff*> * diffs = diff_main(text1, text2, false);
	if (diffs == nullptr){ diffs = new std::vector<Diff*>(); }
	// Convert the diff back to original text.
	diff_charsToLines(diffs, linearray);
	// Eliminate freak matches (e.g. blank lines)
	diff_cleanupSemantic(diffs);

	// Rediff any replacement blocks, this time character-by-character.
	// Add a dummy entry at the end.
	diffs->push_back(new Diff(Operation::EQUAL, std::string("")));
	int pointer = 0;
	int count_delete = 0;
	int count_insert = 0;
	std::string text_delete = "";
	std::string text_insert = "";
	while (pointer < diffs->size())
	{
		switch (diffs->at(pointer)->operation)
		{
		case Operation::INSERT:
			count_insert++;
			text_insert += diffs->at(pointer)->text;
			break;
		case Operation::DELETE:
			count_delete++;
			text_delete += diffs->at(pointer)->text;
			break;
		case Operation::EQUAL:
			// Upon reaching an equality, check for prior redundancies.
			if (count_delete >= 1 && count_insert >= 1) {
				// Delete the offending records and add the merged ones.
				diffs->erase(diffs->begin() + pointer - count_delete - count_insert, diffs->begin() + count_delete + count_insert);
				pointer = pointer - count_delete - count_insert;
				std::vector<Diff*> *  a = diff_main(text_delete, text_insert, false);
				if (a == nullptr){ a = new std::vector<Diff*>(); }
				diffs->insert(diffs->begin() + pointer, a->begin(), a->end());
				pointer = pointer + a->size();
			}
			count_insert = 0;
			count_delete = 0;
			text_delete = "";
			text_insert = "";
			break;
		}
		pointer++;
	}

	diffs->erase(diffs->begin() + diffs->size() - 1);  // Remove the dummy entry at the end.

	return diffs;
}

std::tuple<std::string, std::string, std::vector<std::string>> google_dmp::diff_linesToChars(std::string text1, std::string text2)
{
	std::vector<std::string> lineArray;
	std::unordered_map<std::string, int> lineHash;
	// e.g. linearray[4] == "Hello\n"
	// e.g. linehash.get("Hello\n") == 4

	// "\x00" is a valid character, but various debuggers don't like it.
	// So we'll insert a junk entry to avoid generating a null character.
	lineArray.push_back("");

	std::string chars1 = diff_linesToCharsMunge(text1, lineArray, lineHash);
	std::string chars2 = diff_linesToCharsMunge(text2, lineArray, lineHash);
	return std::make_tuple(chars1, chars2, lineArray);
}

std::string google_dmp::diff_linesToCharsMunge(std::string text, std::vector<std::string> & lineArray, std::unordered_map<std::string, int> & lineHash)
{
	int lineStart = 0;
	int lineEnd = -1;
	std::string line;
	std::string chars;
	// Walk the text, pulling out a Substring for each line.
	// text.split('\n') would would temporarily double our memory footprint.
	// Modifying text would create many large strings to garbage collect.
	while (lineEnd < text.length() - 1)
	{
		lineEnd = text.find('\n', lineStart);
		if (lineEnd == -1)
		{
			lineEnd = text.length() - 1;
		}
		line = text.substr(lineStart, lineEnd + 1 - lineStart);
		lineStart = lineEnd + 1;

		if (lineHash.find(line) != lineHash.end())
		{
			chars += (char)(int)lineHash[line];
		}
		else {
			lineArray.push_back(line);
			lineHash[line] = lineArray.size() - 1;
			chars += ((char)(lineArray.size() - 1));
		}
	}
	return chars;
}

void google_dmp::diff_charsToLines(std::vector<Diff*> * diffs, std::vector<std::string> & lineArray)
{
	std::string text;
	std::vector<Diff*>::iterator _iter = diffs->begin();
	for (; _iter != diffs->end(); _iter++)
	{
		Diff * diff = *_iter;
		text = "";
		for (int y = 0; y < diff->text.length(); y++)
		{
			text += lineArray[diff->text[y]];
		}
		diff->text = text;
	}
}

void google_dmp::diff_cleanupSemantic(std::vector<Diff*> * diffs)
{
	bool changes = false;
	// Stack of indices where equalities are found.
	std::stack<int> equalities;
	// Always equal to equalities[equalitiesLength-1][1]
	std::string lastequality;
	int pointer = 0;  // Index of current position.
	// Number of characters that changed prior to the equality.
	int length_insertions1 = 0;
	int length_deletions1 = 0;
	// Number of characters that changed after the equality.
	int length_insertions2 = 0;
	int length_deletions2 = 0;
	while (pointer < diffs->size())
	{
		if (diffs->at(pointer)->operation == Operation::EQUAL)
		{  // Equality found.
			equalities.push(pointer);
			length_insertions1 = length_insertions2;
			length_deletions1 = length_deletions2;
			length_insertions2 = 0;
			length_deletions2 = 0;
			lastequality = diffs->at(pointer)->text;
		}
		else
		{  // an insertion or deletion
			if (diffs->at(pointer)->operation == Operation::INSERT)
			{
				length_insertions2 += diffs->at(pointer)->text.length();
			}
			else
			{
				length_deletions2 += diffs->at(pointer)->text.length();
			}
			// Eliminate an equality that is smaller or equal to the edits on both
			// sides of it.
			if (!lastequality.empty() && (lastequality.length() <= std::max(length_insertions1, length_deletions1))
				&& (lastequality.length() <= std::max(length_insertions2, length_deletions2)))
			{
				// Duplicate record.
				diffs->insert(diffs->begin() + equalities.top(), new Diff(Operation::DELETE, lastequality));
				// Change second copy to insert.
				diffs->at(equalities.top() + 1)->operation = Operation::INSERT;
				// Throw away the equality we just deleted.
				equalities.pop();
				if (equalities.size() > 0)
				{
					equalities.pop();
				}
				pointer = equalities.size() > 0 ? equalities.top() : -1;
				length_insertions1 = 0;  // Reset the counters.
				length_deletions1 = 0;
				length_insertions2 = 0;
				length_deletions2 = 0;
				lastequality = nullptr;
				changes = true;
			}
		}
		pointer++;
	}

	// Normalize the diff.
	if (changes)
	{
		diff_cleanupMerge(diffs);
	}
	diff_cleanupSemanticLossless(diffs);

	// Find any overlaps between deletions and insertions.
	// e.g: <del>abcxxx</del><ins>xxxdef</ins>
	//   -> <del>abc</del>xxx<ins>def</ins>
	// e.g: <del>xxxabc</del><ins>defxxx</ins>
	//   -> <ins>def</ins>xxx<del>abc</del>
	// Only extract an overlap if it is as big as the edit ahead or behind it.
	pointer = 1;
	while (pointer < diffs->size())
	{
		if (diffs->at(pointer - 1)->operation == Operation::DELETE &&
			diffs->at(pointer)->operation == Operation::INSERT)
		{
			std::string deletion = diffs->at(pointer - 1)->text;
			std::string insertion = diffs->at(pointer)->text;
			int overlap_length1 = diff_commonOverlap(deletion, insertion);
			int overlap_length2 = diff_commonOverlap(insertion, deletion);
			if (overlap_length1 >= overlap_length2) {
				if (overlap_length1 >= deletion.length() / 2.0 ||
					overlap_length1 >= insertion.length() / 2.0) {
					// Overlap found.
					// Insert an equality and trim the surrounding edits.
					diffs->insert(diffs->begin() + pointer, new Diff(Operation::EQUAL, insertion.substr(0, overlap_length1)));
					diffs->at(pointer - 1)->text = deletion.substr(0, deletion.length() - overlap_length1);
					diffs->at(pointer + 1)->text = insertion.substr(overlap_length1);
					pointer++;
				}
			}
			else {
				if (overlap_length2 >= deletion.length() / 2.0 ||
					overlap_length2 >= insertion.length() / 2.0) {
					// Reverse overlap found.
					// Insert an equality and swap and trim the surrounding edits.
					diffs->insert(diffs->begin() + pointer, new Diff(Operation::EQUAL, deletion.substr(0, overlap_length2)));
					diffs->at(pointer - 1)->operation = Operation::INSERT;
					diffs->at(pointer - 1)->text = insertion.substr(0, insertion.length() - overlap_length2);
					diffs->at(pointer + 1)->operation = Operation::DELETE;
					diffs->at(pointer + 1)->text = deletion.substr(overlap_length2);
					pointer++;
				}
			}
			pointer++;
		}
		pointer++;
	}
}

void google_dmp::diff_cleanupMerge(std::vector<Diff*> * diffs)
{
	// Add a dummy entry at the end.
	diffs->push_back(new Diff(Operation::EQUAL, std::string("")));
	int pointer = 0;
	int count_delete = 0;
	int count_insert = 0;
	std::string text_delete;
	std::string text_insert;
	int commonlength;
	while (pointer < diffs->size())
	{
		switch (diffs->at(pointer)->operation)
		{
		case Operation::INSERT:
			count_insert++;
			text_insert += diffs->at(pointer)->text;
			pointer++;
			break;
		case Operation::DELETE:
			count_delete++;
			text_delete += diffs->at(pointer)->text;
			pointer++;
			break;
		case Operation::EQUAL:
			// Upon reaching an equality, check for prior redundancies.
			if (count_delete + count_insert > 1) {
				if (count_delete != 0 && count_insert != 0) {
					// Factor out any common prefixies.
					commonlength = diff_commonPrefix(text_insert, text_delete);
					if (commonlength != 0) {
						if ((pointer - count_delete - count_insert) > 0 &&
							diffs->at(pointer - count_delete - count_insert - 1)->operation == Operation::EQUAL)
						{
							diffs->at(pointer - count_delete - count_insert - 1)->text += text_insert.substr(0, commonlength);
						}
						else
						{
							diffs->insert(diffs->begin() + 0, new Diff(Operation::EQUAL, text_insert.substr(0, commonlength)));
							pointer++;
						}
						text_insert = text_insert.substr(commonlength);
						text_delete = text_delete.substr(commonlength);
					}
					// Factor out any common suffixies.
					commonlength = diff_commonSuffix(text_insert, text_delete);
					if (commonlength != 0)
					{
						diffs->at(pointer)->text = text_insert.substr(text_insert.length() - commonlength) + diffs->at(pointer)->text;
						text_insert = text_insert.substr(0, text_insert.length() - commonlength);
						text_delete = text_delete.substr(0, text_delete.length() - commonlength);
					}
				}
				// Delete the offending records and add the merged ones.
				if (count_delete == 0)
				{
					SpliceIL<Diff*>(diffs, pointer - count_insert, count_delete + count_insert,
					{ new Diff(Operation::INSERT, text_insert)
					});
				}
				else if (count_insert == 0)
				{
					SpliceIL<Diff*>(diffs, pointer - count_delete, count_delete + count_insert,
					{ new Diff(Operation::DELETE, text_delete)
					});
				}
				else
				{
					SpliceIL<Diff*>(diffs, pointer - count_delete - count_insert, count_delete + count_insert,
					{ new Diff(Operation::DELETE, text_delete),
					new Diff(Operation::INSERT, text_insert)
					});
				}
				pointer = pointer - count_delete - count_insert +
					(count_delete != 0 ? 1 : 0) + (count_insert != 0 ? 1 : 0) + 1;
			}
			else if (pointer != 0
				&& diffs->at(pointer - 1)->operation == Operation::EQUAL)
			{
				// Merge this equality with the previous one.
				diffs->at(pointer - 1)->text += diffs->at(pointer)->text;
				diffs->erase(diffs->begin() + pointer);
			}
			else {
				pointer++;
			}
			count_insert = 0;
			count_delete = 0;
			text_delete = "";
			text_insert = "";
			break;
		}
	}
	if (diffs->at(diffs->size() - 1)->text.length() == 0)
	{
		diffs->erase(diffs->begin() + diffs->size() - 1);// Remove the dummy entry at the end.
	}

	// Second pass: look for single edits surrounded on both sides by
	// equalities which can be shifted sideways to eliminate an equality.
	// e.g: A<ins>BA</ins>C -> <ins>AB</ins>AC
	bool changes = false;
	pointer = 1;
	// Intentionally ignore the first and last element (don't need checking).
	while (pointer < (diffs->size() - 1))
	{
		if (diffs->at(pointer - 1)->operation == Operation::EQUAL &&
			diffs->at(pointer + 1)->operation == Operation::EQUAL) {
			// This is a single edit surrounded by equalities.
			unsigned found = diffs->at(pointer)->text.rfind(diffs->at(pointer - 1)->text);
			if (found != std::string::npos && diffs->at(pointer)->text.substr(found) == diffs->at(pointer - 1)->text) //!----endwith
			{
				// Shift the edit over the previous equality.
				diffs->at(pointer)->text = diffs->at(pointer - 1)->text +
					diffs->at(pointer)->text.substr(0, diffs->at(pointer)->text.length() -
					diffs->at(pointer - 1)->text.length());
				diffs->at(pointer + 1)->text = diffs->at(pointer - 1)->text
					+ diffs->at(pointer + 1)->text;
				SpliceIL<Diff*>(diffs, pointer - 1, 1, {});
				changes = true;
			}
			else if (diffs->at(pointer)->text.find(diffs->at(pointer + 1)->text) == 0)
			{
				// Shift the edit over the next equality.
				diffs->at(pointer - 1)->text += diffs->at(pointer + 1)->text;
				diffs->at(pointer)->text =
					diffs->at(pointer)->text.substr(diffs->at(pointer + 1)->text.length())
					+ diffs->at(pointer + 1)->text;
				SpliceIL<Diff*>(diffs, pointer + 1, 1, {});
				changes = true;
			}
		}
		pointer++;
	}
	// If shifts were made, the diff needs reordering and another shift sweep.
	if (changes) {
		diff_cleanupMerge(diffs);
	}
}

void google_dmp::diff_cleanupSemanticLossless(std::vector<Diff*> * diffs)
{
	int pointer = 1;
	// Intentionally ignore the first and last element (don't need checking).
	while (pointer < diffs->size() - 1) {
		if (diffs->at(pointer - 1)->operation == Operation::EQUAL &&
			diffs->at(pointer + 1)->operation == Operation::EQUAL) {
			// This is a single edit surrounded by equalities.
			std::string equality1 = diffs->at(pointer - 1)->text;
			std::string edit = diffs->at(pointer)->text;
			std::string equality2 = diffs->at(pointer + 1)->text;

			// First, shift the edit as far left as possible.
			int commonOffset = diff_commonSuffix(equality1, edit);
			if (commonOffset > 0) {
				std::string commonString = edit.substr(edit.length() - commonOffset);
				equality1 = equality1.substr(0, equality1.length() - commonOffset);
				edit = commonString + edit.substr(0, edit.length() - commonOffset);
				equality2 = commonString + equality2;
			}

			// Second, step character by character right,
			// looking for the best fit.
			std::string bestEquality1 = equality1;
			std::string bestEdit = edit;
			std::string bestEquality2 = equality2;
			int bestScore = diff_cleanupSemanticScore(equality1, edit) +
				diff_cleanupSemanticScore(edit, equality2);
			while (edit.length() != 0 && equality2.length() != 0
				&& edit[0] == equality2[0]) {
				equality1 += edit[0];
				edit = edit.substr(1) + equality2[0];
				equality2 = equality2.substr(1);
				int score = diff_cleanupSemanticScore(equality1, edit) +
					diff_cleanupSemanticScore(edit, equality2);
				// The >= encourages trailing rather than leading whitespace on
				// edits.
				if (score >= bestScore) {
					bestScore = score;
					bestEquality1 = equality1;
					bestEdit = edit;
					bestEquality2 = equality2;
				}
			}

			if (diffs->at(pointer - 1)->text != bestEquality1) {
				// We have an improvement, save it back to the diff.
				if (bestEquality1.length() != 0) {
					diffs->at(pointer - 1)->text = bestEquality1;
				}
				else {
					diffs->erase(diffs->begin() + pointer - 1);
					pointer--;
				}
				diffs->at(pointer)->text = bestEdit;
				if (bestEquality2.length() != 0) {
					diffs->at(pointer + 1)->text = bestEquality2;
				}
				else {
					diffs->erase(diffs->begin() + pointer + 1);
					pointer--;
				}
			}
		}
		pointer++;
	}
}

int google_dmp::diff_commonOverlap(std::string text1, std::string text2)
{
	// Cache the text lengths to prevent multiple calls.
	int text1_length = text1.length();
	int text2_length = text2.length();
	// Eliminate the null case.
	if (text1_length == 0 || text2_length == 0) {
		return 0;
	}
	// Truncate the longer string.
	if (text1_length > text2_length) {
		text1 = text1.substr(text1_length - text2_length);
	}
	else if (text1_length < text2_length) {
		text2 = text2.substr(0, text1_length);
	}
	int text_length = std::min(text1_length, text2_length);
	// Quick check for the worst case.
	if (text1 == text2) {
		return text_length;
	}

	int best = 0;
	int length = 1;
	while (true) {
		std::string pattern = text1.substr(text_length - length);
		int found = text2.find(pattern);
		if (found == -1) {
			return best;
		}
		length += found;
		if (found == 0 || text1.substr(text_length - length) ==
			text2.substr(0, length)) {
			best = length;
			length++;
		}
	}
}

std::vector<Diff*> * google_dmp::diff_bisect(std::string text1, std::string text2, std::time_t deadline)
{
	// Cache the text lengths to prevent multiple calls.
	int text1_length = text1.length();
	int text2_length = text2.length();
	int max_d = (text1_length + text2_length + 1) / 2;
	int v_offset = max_d;
	int v_length = 2 * max_d;
	int* v1 = new int[v_length];
	int* v2 = new int[v_length];
	for (int x = 0; x < v_length; x++) {
		v1[x] = -1;
		v2[x] = -1;
	}
	v1[v_offset + 1] = 0;
	v2[v_offset + 1] = 0;
	int delta = text1_length - text2_length;
	// If the total number of characters is odd, then the front path will
	// collide with the reverse path.
	bool front = (delta % 2 != 0);
	// Offsets for start and end of k loop.
	// Prevents mapping of space beyond the grid.
	int k1start = 0;
	int k1end = 0;
	int k2start = 0;
	int k2end = 0;
	for (int d = 0; d < max_d; d++) {
		// Bail out if deadline is reached.
		if (std::time(nullptr) > deadline) {
			break;
		}

		// Walk the front path one step.
		for (int k1 = -d + k1start; k1 <= d - k1end; k1 += 2) {
			int k1_offset = v_offset + k1;
			int x1;
			if (k1 == -d || k1 != d && v1[k1_offset - 1] < v1[k1_offset + 1]) {
				x1 = v1[k1_offset + 1];
			}
			else {
				x1 = v1[k1_offset - 1] + 1;
			}
			int y1 = x1 - k1;
			while (x1 < text1_length && y1 < text2_length
				&& text1[x1] == text2[y1]) {
				x1++;
				y1++;
			}
			v1[k1_offset] = x1;
			if (x1 > text1_length) {
				// Ran off the right of the graph.
				k1end += 2;
			}
			else if (y1 > text2_length) {
				// Ran off the bottom of the graph.
				k1start += 2;
			}
			else if (front) {
				int k2_offset = v_offset + delta - k1;
				if (k2_offset >= 0 && k2_offset < v_length && v2[k2_offset] != -1) {
					// Mirror x2 onto top-left coordinate system.
					int x2 = text1_length - v2[k2_offset];
					if (x1 >= x2) {
						// Overlap detected.
						return diff_bisectSplit(text1, text2, x1, y1, deadline);
					}
				}
			}
		}

		// Walk the reverse path one step.
		for (int k2 = -d + k2start; k2 <= d - k2end; k2 += 2) {
			int k2_offset = v_offset + k2;
			int x2;
			if (k2 == -d || k2 != d && v2[k2_offset - 1] < v2[k2_offset + 1]) {
				x2 = v2[k2_offset + 1];
			}
			else {
				x2 = v2[k2_offset - 1] + 1;
			}
			int y2 = x2 - k2;
			while (x2 < text1_length && y2 < text2_length
				&& text1[text1_length - x2 - 1]
				== text2[text2_length - y2 - 1]) {
				x2++;
				y2++;
			}
			v2[k2_offset] = x2;
			if (x2 > text1_length) {
				// Ran off the left of the graph.
				k2end += 2;
			}
			else if (y2 > text2_length) {
				// Ran off the top of the graph.
				k2start += 2;
			}
			else if (!front) {
				int k1_offset = v_offset + delta - k2;
				if (k1_offset >= 0 && k1_offset < v_length && v1[k1_offset] != -1) {
					int x1 = v1[k1_offset];
					int y1 = v_offset + x1 - k1_offset;
					// Mirror x2 onto top-left coordinate system.
					x2 = text1_length - v2[k2_offset];
					if (x1 >= x2) {
						// Overlap detected.
						return diff_bisectSplit(text1, text2, x1, y1, deadline);
					}
				}
			}
		}
	}
	// Diff took too long and hit the deadline or
	// number of diffs equals number of characters, no commonality at all.
	std::vector<Diff*> * diffs = new std::vector<Diff*>();
	diffs->push_back(new Diff(Operation::DELETE, text1));
	diffs->push_back(new Diff(Operation::INSERT, text2));
	return diffs;
}

std::vector<Diff*> * google_dmp::diff_bisectSplit(std::string text1, std::string text2, int x, int y, std::time_t deadline)
{
	std::string text1a = text1.substr(0, x);
	std::string text2a = text2.substr(0, y);
	std::string text1b = text1.substr(x);
	std::string text2b = text2.substr(y);

	// Compute both diffs serially.
	std::vector<Diff*> * diffs = diff_main(text1a, text2a, false);
	std::vector<Diff*> * diffsb = diff_main(text1b, text2b, false);

	if (diffs == nullptr) { diffs = new std::vector<Diff*>(); }
	diffs->insert(diffs->end(), diffsb->begin(), diffsb->end());
	return diffs;
}

int google_dmp::diff_cleanupSemanticScore(std::string one, std::string two)
{
	if (one.length() == 0 || two.length() == 0) {
		// Edges are the best.
		return 6;
	}

	// Each port of this function behaves slightly differently due to
	// subtle differences in each language's definition of things like
	// 'whitespace'.  Since this function's purpose is largely cosmetic,
	// the choice has been made to use each language's native features
	// rather than force total conformity.
	char char1 = one[one.length() - 1];
	char char2 = two[0];
	bool nonAlphaNumeric1 = !isalpha(char1);
	bool nonAlphaNumeric2 = !isalpha(char2);
	bool whitespace1 = nonAlphaNumeric1 && isspace(char1);
	bool whitespace2 = nonAlphaNumeric2 && isspace(char2);
	bool lineBreak1 = whitespace1 && isblank(char1);
	bool lineBreak2 = whitespace2 && isblank(char2);
	bool blankLine1 = lineBreak1 && isBlankLine(one);
	bool blankLine2 = lineBreak2 && isBlankLine(two);

	if (blankLine1 || blankLine2) {
		// Five points for blank lines.
		return 5;
	}
	else if (lineBreak1 || lineBreak2) {
		// Four points for line breaks.
		return 4;
	}
	else if (nonAlphaNumeric1 && !whitespace1 && whitespace2) {
		// Three points for end of sentences.
		return 3;
	}
	else if (whitespace1 || whitespace2) {
		// Two points for whitespace.
		return 2;
	}
	else if (nonAlphaNumeric1 || nonAlphaNumeric2) {
		// One point for non-alphanumeric.
		return 1;
	}
	return 0;
}
