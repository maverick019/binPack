#include <algorithm>
#include <utility>
#include <iostream>
#include <limits>
#include<fstream>
#include <ostream>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cmath>
#include "RectLoader.h"

namespace rbp {

	bool IsContainedIn(const Rect& a, const Rect& b)
	{
		return a.x >= b.x && a.y >= b.y
			&& a.x + a.width <= b.x + b.width
			&& a.y + a.height <= b.y + b.height;
	} 

	using namespace std;

	RectLoader::RectLoader() :binWidth(0), binHeight(0)
	{
	}

	RectLoader::RectLoader(int width, int height, bool allowFlip)
	{
		Init(width, height, allowFlip);
	} 
	
	RectLoader::RectLoader(const std::string& filePath)
	{
		std::ifstream inFile;
		inFile.open(filePath);
		Rect temp;
		if (!inFile) {
			std::cerr << "Unable to open file datafile.txt";
			exit(1);   // call system to stop
		} 
		while (inFile >> temp.height >> temp.width)
		{
			listRec.push_back(temp);
		}
	}

	std::vector<Rect> RectLoader::getList()
	{
		return listRec;
	}

	void RectLoader::Init(int width, int height, bool allowFlip)
	{
		binAllowFlip = allowFlip;
		binWidth = width;
		binHeight = height;

		Rect n;
		n.x = 0;
		n.y = 0;
		n.width = width;
		n.height = height;

		usedRectangles.clear();

		freeRectangles.clear();
		freeRectangles.push_back(n);
	}

	Rect RectLoader::Insert(int width, int height, FreeRectChoiceHeuristic method)
	{
		Rect newNode;
		// Unused in this function. We don't need to know the score after finding the position.
		int score1 = std::numeric_limits<int>::max();
		int score2 = std::numeric_limits<int>::max();
		switch (method)
		{
		case RectBestShortSideFit: newNode = FindPositionForNewNodeBestShortSideFit(width, height, score1, score2); break;
		case RectBottomLeftRule: newNode = FindPositionForNewNodeBottomLeft(width, height, score1, score2); break;
		case RectContactPointRule: newNode = FindPositionForNewNodeContactPoint(width, height, score1); break;
		case RectBestLongSideFit: newNode = FindPositionForNewNodeBestLongSideFit(width, height, score2, score1); break;
		case RectBestAreaFit: newNode = FindPositionForNewNodeBestAreaFit(width, height, score1, score2); break;
		}

		if (newNode.height == 0)
			return newNode;

		size_t numRectanglesToProcess = freeRectangles.size();
		for (size_t i = 0; i < numRectanglesToProcess; ++i)
		{
			if (SplitFreeNode(freeRectangles[i], newNode))
			{
				freeRectangles.erase(freeRectangles.begin() + i);
				--i;
				--numRectanglesToProcess;
			}
		}

		PruneFreeList();

		usedRectangles.push_back(newNode);
		return newNode;
	}

	void RectLoader::Insert(std::vector<RectSize>& rects, std::vector<Rect>& dst, FreeRectChoiceHeuristic method)
	{
		dst.clear();

		while (rects.size() > 0)
		{
			int bestScore1 = std::numeric_limits<int>::max();
			int bestScore2 = std::numeric_limits<int>::max();
			int bestRectIndex = -1;
			Rect bestNode;

			for (size_t i = 0; i < rects.size(); ++i)
			{
				int score1;
				int score2;
				Rect newNode = ScoreRect(rects[i].width, rects[i].height, method, score1, score2);

				if (score1 < bestScore1 || (score1 == bestScore1 && score2 < bestScore2))
				{
					bestScore1 = score1;
					bestScore2 = score2;
					bestNode = newNode;
					bestRectIndex = i;
				}
			}

			if (bestRectIndex == -1)
				return;

			PlaceRect(bestNode);
			dst.push_back(bestNode);
			rects.erase(rects.begin() + bestRectIndex);
		}
	}

	void RectLoader::PlaceRect(const Rect& node)
	{
		size_t numRectanglesToProcess = freeRectangles.size();
		for (size_t i = 0; i < numRectanglesToProcess; ++i)
		{
			if (SplitFreeNode(freeRectangles[i], node))
			{
				freeRectangles.erase(freeRectangles.begin() + i);
				--i;
				--numRectanglesToProcess;
			}
		}

		PruneFreeList();

		usedRectangles.push_back(node);
	}

	Rect RectLoader::ScoreRect(int width, int height, FreeRectChoiceHeuristic method, int& score1, int& score2) const
	{
		Rect newNode;
		score1 = std::numeric_limits<int>::max();
		score2 = std::numeric_limits<int>::max();
		switch (method)
		{
		case RectBestShortSideFit: newNode = FindPositionForNewNodeBestShortSideFit(width, height, score1, score2); break;
		case RectBottomLeftRule: newNode = FindPositionForNewNodeBottomLeft(width, height, score1, score2); break;
		case RectContactPointRule: newNode = FindPositionForNewNodeContactPoint(width, height, score1);
			score1 = -score1; // Reverse since we are minimizing, but for contact point score bigger is better.
			break;
		case RectBestLongSideFit: newNode = FindPositionForNewNodeBestLongSideFit(width, height, score2, score1); break;
		case RectBestAreaFit: newNode = FindPositionForNewNodeBestAreaFit(width, height, score1, score2); break;
		}

		// Cannot fit the current rectangle.
		if (newNode.height == 0)
		{
			score1 = std::numeric_limits<int>::max();
			score2 = std::numeric_limits<int>::max();
		}

		return newNode;
	}

	/// Computes the ratio of used surface area.
	float RectLoader::Occupancy() const
	{
		unsigned long usedSurfaceArea = 0;
		for (size_t i = 0; i < usedRectangles.size(); ++i)
			usedSurfaceArea += usedRectangles[i].width * usedRectangles[i].height;

		return (float)usedSurfaceArea / (binWidth * binHeight);
	}

	Rect RectLoader::FindPositionForNewNodeBottomLeft(int width, int height, int& bestY, int& bestX) const
	{
		Rect bestNode;
		memset(&bestNode, 0, sizeof(Rect));

		bestY = std::numeric_limits<int>::max();
		bestX = std::numeric_limits<int>::max();

		for (size_t i = 0; i < freeRectangles.size(); ++i)
		{
			// Try to place the rectangle in upright (non-flipped) orientation.
			if (freeRectangles[i].width >= width && freeRectangles[i].height >= height)
			{
				int topSideY = freeRectangles[i].y + height;
				if (topSideY < bestY || (topSideY == bestY && freeRectangles[i].x < bestX))
				{
					bestNode.x = freeRectangles[i].x;
					bestNode.y = freeRectangles[i].y;
					bestNode.width = width;
					bestNode.height = height;
					bestY = topSideY;
					bestX = freeRectangles[i].x;
				}
			}
			if (binAllowFlip && freeRectangles[i].width >= height && freeRectangles[i].height >= width)
			{
				int topSideY = freeRectangles[i].y + width;
				if (topSideY < bestY || (topSideY == bestY && freeRectangles[i].x < bestX))
				{
					bestNode.x = freeRectangles[i].x;
					bestNode.y = freeRectangles[i].y;
					bestNode.width = height;
					bestNode.height = width;
					bestY = topSideY;
					bestX = freeRectangles[i].x;
				}
			}
		}
		return bestNode;
	}

	Rect RectLoader::FindPositionForNewNodeBestShortSideFit(int width, int height,
		int& bestShortSideFit, int& bestLongSideFit) const
	{
		Rect bestNode;
		memset(&bestNode, 0, sizeof(Rect));

		bestShortSideFit = std::numeric_limits<int>::max();
		bestLongSideFit = std::numeric_limits<int>::max();

		for (size_t i = 0; i < freeRectangles.size(); ++i)
		{
			// Try to place the rectangle in upright (non-flipped) orientation.
			if (freeRectangles[i].width >= width && freeRectangles[i].height >= height)
			{
				int leftoverHoriz = abs(freeRectangles[i].width - width);
				int leftoverVert = abs(freeRectangles[i].height - height);
				int shortSideFit = min(leftoverHoriz, leftoverVert);
				int longSideFit = max(leftoverHoriz, leftoverVert);

				if (shortSideFit < bestShortSideFit || (shortSideFit == bestShortSideFit && longSideFit < bestLongSideFit))
				{
					bestNode.x = freeRectangles[i].x;
					bestNode.y = freeRectangles[i].y;
					bestNode.width = width;
					bestNode.height = height;
					bestShortSideFit = shortSideFit;
					bestLongSideFit = longSideFit;
				}
			}

			if (binAllowFlip && freeRectangles[i].width >= height && freeRectangles[i].height >= width)
			{
				int flippedLeftoverHoriz = abs(freeRectangles[i].width - height);
				int flippedLeftoverVert = abs(freeRectangles[i].height - width);
				int flippedShortSideFit = min(flippedLeftoverHoriz, flippedLeftoverVert);
				int flippedLongSideFit = max(flippedLeftoverHoriz, flippedLeftoverVert);

				if (flippedShortSideFit < bestShortSideFit || (flippedShortSideFit == bestShortSideFit && flippedLongSideFit < bestLongSideFit))
				{
					bestNode.x = freeRectangles[i].x;
					bestNode.y = freeRectangles[i].y;
					bestNode.width = height;
					bestNode.height = width;
					bestShortSideFit = flippedShortSideFit;
					bestLongSideFit = flippedLongSideFit;
				}
			}
		}
		return bestNode;
	}

	Rect RectLoader::FindPositionForNewNodeBestLongSideFit(int width, int height,
		int& bestShortSideFit, int& bestLongSideFit) const
	{
		Rect bestNode;
		memset(&bestNode, 0, sizeof(Rect));

		bestShortSideFit = std::numeric_limits<int>::max();
		bestLongSideFit = std::numeric_limits<int>::max();

		for (size_t i = 0; i < freeRectangles.size(); ++i)
		{
			// Try to place the rectangle in upright (non-flipped) orientation.
			if (freeRectangles[i].width >= width && freeRectangles[i].height >= height)
			{
				int leftoverHoriz = abs(freeRectangles[i].width - width);
				int leftoverVert = abs(freeRectangles[i].height - height);
				int shortSideFit = min(leftoverHoriz, leftoverVert);
				int longSideFit = max(leftoverHoriz, leftoverVert);

				if (longSideFit < bestLongSideFit || (longSideFit == bestLongSideFit && shortSideFit < bestShortSideFit))
				{
					bestNode.x = freeRectangles[i].x;
					bestNode.y = freeRectangles[i].y;
					bestNode.width = width;
					bestNode.height = height;
					bestShortSideFit = shortSideFit;
					bestLongSideFit = longSideFit;
				}
			}

			if (binAllowFlip && freeRectangles[i].width >= height && freeRectangles[i].height >= width)
			{
				int leftoverHoriz = abs(freeRectangles[i].width - height);
				int leftoverVert = abs(freeRectangles[i].height - width);
				int shortSideFit = min(leftoverHoriz, leftoverVert);
				int longSideFit = max(leftoverHoriz, leftoverVert);

				if (longSideFit < bestLongSideFit || (longSideFit == bestLongSideFit && shortSideFit < bestShortSideFit))
				{
					bestNode.x = freeRectangles[i].x;
					bestNode.y = freeRectangles[i].y;
					bestNode.width = height;
					bestNode.height = width;
					bestShortSideFit = shortSideFit;
					bestLongSideFit = longSideFit;
				}
			}
		}
		return bestNode;
	}

	Rect RectLoader::FindPositionForNewNodeBestAreaFit(int width, int height,
		int& bestAreaFit, int& bestShortSideFit) const
	{
		Rect bestNode;
		memset(&bestNode, 0, sizeof(Rect));

		bestAreaFit = std::numeric_limits<int>::max();
		bestShortSideFit = std::numeric_limits<int>::max();

		for (size_t i = 0; i < freeRectangles.size(); ++i)
		{
			int areaFit = freeRectangles[i].width * freeRectangles[i].height - width * height;

			// Try to place the rectangle in upright (non-flipped) orientation.
			if (freeRectangles[i].width >= width && freeRectangles[i].height >= height)
			{
				int leftoverHoriz = abs(freeRectangles[i].width - width);
				int leftoverVert = abs(freeRectangles[i].height - height);
				int shortSideFit = min(leftoverHoriz, leftoverVert);

				if (areaFit < bestAreaFit || (areaFit == bestAreaFit && shortSideFit < bestShortSideFit))
				{
					bestNode.x = freeRectangles[i].x;
					bestNode.y = freeRectangles[i].y;
					bestNode.width = width;
					bestNode.height = height;
					bestShortSideFit = shortSideFit;
					bestAreaFit = areaFit;
				}
			}

			if (binAllowFlip && freeRectangles[i].width >= height && freeRectangles[i].height >= width)
			{
				int leftoverHoriz = abs(freeRectangles[i].width - height);
				int leftoverVert = abs(freeRectangles[i].height - width);
				int shortSideFit = min(leftoverHoriz, leftoverVert);

				if (areaFit < bestAreaFit || (areaFit == bestAreaFit && shortSideFit < bestShortSideFit))
				{
					bestNode.x = freeRectangles[i].x;
					bestNode.y = freeRectangles[i].y;
					bestNode.width = height;
					bestNode.height = width;
					bestShortSideFit = shortSideFit;
					bestAreaFit = areaFit;
				}
			}
		}
		return bestNode;
	}

	/// Returns 0 if the two intervals i1 and i2 are disjoint, or the length of their overlap otherwise.
	int CommonIntervalLength(int i1start, int i1end, int i2start, int i2end)
	{
		if (i1end < i2start || i2end < i1start)
			return 0;
		return min(i1end, i2end) - max(i1start, i2start);
	}

	int RectLoader::ContactPointScoreNode(int x, int y, int width, int height) const
	{
		int score = 0;

		if (x == 0 || x + width == binWidth)
			score += height;
		if (y == 0 || y + height == binHeight)
			score += width;

		for (size_t i = 0; i < usedRectangles.size(); ++i)
		{
			if (usedRectangles[i].x == x + width || usedRectangles[i].x + usedRectangles[i].width == x)
				score += CommonIntervalLength(usedRectangles[i].y, usedRectangles[i].y + usedRectangles[i].height, y, y + height);
			if (usedRectangles[i].y == y + height || usedRectangles[i].y + usedRectangles[i].height == y)
				score += CommonIntervalLength(usedRectangles[i].x, usedRectangles[i].x + usedRectangles[i].width, x, x + width);
		}
		return score;
	}

	Rect RectLoader::FindPositionForNewNodeContactPoint(int width, int height, int& bestContactScore) const
	{
		Rect bestNode;
		memset(&bestNode, 0, sizeof(Rect));

		bestContactScore = -1;

		for (size_t i = 0; i < freeRectangles.size(); ++i)
		{
			// Try to place the rectangle in upright (non-flipped) orientation.
			if (freeRectangles[i].width >= width && freeRectangles[i].height >= height)
			{
				int score = ContactPointScoreNode(freeRectangles[i].x, freeRectangles[i].y, width, height);
				if (score > bestContactScore)
				{
					bestNode.x = freeRectangles[i].x;
					bestNode.y = freeRectangles[i].y;
					bestNode.width = width;
					bestNode.height = height;
					bestContactScore = score;
				}
			}
			if (freeRectangles[i].width >= height && freeRectangles[i].height >= width)
			{
				int score = ContactPointScoreNode(freeRectangles[i].x, freeRectangles[i].y, height, width);
				if (score > bestContactScore)
				{
					bestNode.x = freeRectangles[i].x;
					bestNode.y = freeRectangles[i].y;
					bestNode.width = height;
					bestNode.height = width;
					bestContactScore = score;
				}
			}
		}
		return bestNode;
	}

	bool RectLoader::SplitFreeNode(Rect freeNode, const Rect& usedNode)
	{
		// Test with SAT if the rectangles even intersect.
		if (usedNode.x >= freeNode.x + freeNode.width || usedNode.x + usedNode.width <= freeNode.x ||
			usedNode.y >= freeNode.y + freeNode.height || usedNode.y + usedNode.height <= freeNode.y)
			return false;

		if (usedNode.x < freeNode.x + freeNode.width && usedNode.x + usedNode.width > freeNode.x)
		{
			// New node at the top side of the used node.
			if (usedNode.y > freeNode.y && usedNode.y < freeNode.y + freeNode.height)
			{
				Rect newNode = freeNode;
				newNode.height = usedNode.y - newNode.y;
				freeRectangles.push_back(newNode);
			}

			// New node at the bottom side of the used node.
			if (usedNode.y + usedNode.height < freeNode.y + freeNode.height)
			{
				Rect newNode = freeNode;
				newNode.y = usedNode.y + usedNode.height;
				newNode.height = freeNode.y + freeNode.height - (usedNode.y + usedNode.height);
				freeRectangles.push_back(newNode);
			}
		}

		if (usedNode.y < freeNode.y + freeNode.height && usedNode.y + usedNode.height > freeNode.y)
		{
			// New node at the left side of the used node.
			if (usedNode.x > freeNode.x && usedNode.x < freeNode.x + freeNode.width)
			{
				Rect newNode = freeNode;
				newNode.width = usedNode.x - newNode.x;
				freeRectangles.push_back(newNode);
			}

			// New node at the right side of the used node.
			if (usedNode.x + usedNode.width < freeNode.x + freeNode.width)
			{
				Rect newNode = freeNode;
				newNode.x = usedNode.x + usedNode.width;
				newNode.width = freeNode.x + freeNode.width - (usedNode.x + usedNode.width);
				freeRectangles.push_back(newNode);
			}
		}

		return true;
	}

	void RectLoader::PruneFreeList()
	{
		/// Go through each pair and remove any rectangle that is redundant.
		for (size_t i = 0; i < freeRectangles.size(); ++i)
			for (size_t j = i + 1; j < freeRectangles.size(); ++j)
			{
				if (IsContainedIn(freeRectangles[i], freeRectangles[j]))
				{
					freeRectangles.erase(freeRectangles.begin() + i);
					--i;
					break;
				}
				if (IsContainedIn(freeRectangles[j], freeRectangles[i]))
				{
					freeRectangles.erase(freeRectangles.begin() + j);
					--j;
				}
			}
	}

}

int main(int argc, char** argv)
{
	using namespace rbp; 
	// Create a bin to pack to, use the bin size defined.
	RectLoader bin = RectLoader("rectangles.txt");
	std::cout << "Enter Square dimensions" << std::endl;
	int binWidth, binHeight; 
	// commandline input: width height
	std::cin >> binWidth >> binHeight;
	if (binWidth != binHeight)
	{
		std::cerr << "Entered value is not a valid square dimension";
	}
	printf("Initializing bin to size %dx%d.\n", binWidth, binHeight);
	bin.Init(binWidth, binHeight);
	std::vector<Rect> finalList = bin.getList();

	// Pack each rectangle (w_i, h_i) the user inputted on the command line.
	for (std::vector<Rect>::iterator it = finalList.begin(); it != finalList.end(); it++)
	{
		// Read next rectangle to pack.
		int rectWidth = it->width;
		int rectHeight = it->height;
		printf("Packing rectangle of size %dx%d: ", rectWidth, rectHeight);

		// Perform the packing.
		RectLoader::FreeRectChoiceHeuristic heuristic = RectLoader::RectBestShortSideFit; // This can be changed individually even for each rectangle packed.
		Rect packedRect = bin.Insert(rectWidth, rectHeight, heuristic);

		// Test success or failure.
		if (packedRect.height > 0)
			printf("Packed to (x,y)=(%d,%d), (w,h)=(%d,%d). Free space left: %.2f%%\n", packedRect.x, packedRect.y, packedRect.width, packedRect.height, 100.f - bin.Occupancy() * 100.f);
		else
			printf("Failed! Could not find a proper position to pack this rectangle into. Skipping this one.\n");
	}
	printf("Done. All rectangles packed.\n");
}




