#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <algorithm>

using namespace cv;
using namespace std;

// Constants
// DefaultMinDist = gray.rows / 8 . DefaultParam2 is important
const int DefaultMinDist = 60, DefaultParam1 = 85, DefaultParam2 = 35, DefaultMinRadius = 45,
          DefaultRadmaxius = 80, MaxValue = 255;

// Global variables
Mat frame, dst, output, ROI, polarImg_Inv, diff, grayDiff, mergeIMG, srcClone;
vector<Vec3f> circles;
int minDist, param1, param2, minRadius, Radmaxius, channel;
int circleCenterX, circleCenterY, circleRadius, pixelValue = 155, brightness = 155, travel = 0;
Point prevMousePosition(-1, -1);

// Function prototypes
void minDistCall(int, void *);
void param1Call(int, void *);
void param2Call(int, void *);
void minRadiusCall(int, void *);
void RadmaxiusCall(int, void *);
void channelCall(int, void *);
void calibration(const Mat &input);
void initializeTrackbars();
void calcCircles(const Mat &input, vector<Vec3f> &circles);
void detectCirclesAndDraw();
bool isCircleOutOfBounds(int centerX, int centerY, int radius, int maxWidth, int maxHeight);
void circleDetect(double &defectSize);
void imgMerge(Mat &srcClone, double &defectSize);

int main(int argc, char **argv)
{
    // -------------------------------------------------------------------------
    // ---                        HoughCircles                               ---
    // -------------------------------------------------------------------------
    // pre-process
    VideoCapture capture("..\\data\\video\\0105.avi");
    if (!capture.isOpened())
    {
        cout << "Cannot open the video." << endl;
        return -1;
    }
    capture.read(frame);
    // src = imread("C:\\Users\\ASUS\\Desktop\\img\\345.bmp");
    calibration(frame);
    cvtColor(dst, dst, COLOR_BGR2GRAY);
    medianBlur(dst, dst, 3);
    // threshold(dst, dst, 220, 255, THRESH_BINARY);
    threshold(dst, dst, 0, 255, THRESH_OTSU);
    // windows
    namedWindow("output", WINDOW_AUTOSIZE);
    initializeTrackbars();

    while (1)
    {
        detectCirclesAndDraw();

        int key = waitKey(1);
        if (key == 'q' || key == 27)
        {
            break;
        }
    }
    destroyAllWindows();
    // -------------------------------------------------------------------------
    // ---      use polar coodiates transfer detect circle defect            ---
    // -------------------------------------------------------------------------
    namedWindow("merge", WINDOW_AUTOSIZE);
    const int InitChannel = 0, MaxChannel = circles.size() - 1;
    bool hasDefect = false;
    double defectSize;
    createTrackbar("channel", "merge", nullptr, MaxChannel, channelCall);
    setTrackbarPos("channel", "merge", InitChannel);

    while (capture.read(frame))
    {
        srcClone = frame.clone();
        calcCircles(frame, circles);
        while (1)
        {
            defectSize = 0;
            float bias = 1.0;
            if (travel < circles.size())
            {
                circleCenterX = cvRound(circles[travel][0]);
                circleCenterY = cvRound(circles[travel][1]);
                circleRadius = cvRound(circles[travel][2]) * bias;
                if (isCircleOutOfBounds(circleCenterX, circleCenterY, circleRadius, frame.cols, frame.rows))
                    break;

                circleDetect(defectSize);
                imgMerge(srcClone, defectSize);

                travel++;
            }
            else
            {
                circleCenterX = cvRound(circles[channel][0]);
                circleCenterY = cvRound(circles[channel][1]);
                circleRadius = cvRound(circles[channel][2]) * bias;
                if (!isCircleOutOfBounds(circleCenterX, circleCenterY, circleRadius, frame.cols, frame.rows))
                {
                    circleDetect(defectSize);
                    imgMerge(srcClone, defectSize);
                }
                int key = waitKey(1);
                travel = 0;
                break;
            }
        }
        int key = waitKey(1);
        if (key == 'q' || key == 27)
        {
            break;
        }
    }
    destroyAllWindows();
    return 0;
}

// -------------------------------------------------------------------------e
// ---                        call back function                         ---
// -------------------------------------------------------------------------
void minDistCall(int, void *)
{
    minDist = getTrackbarPos("minDist", "output");
}
void param1Call(int, void *)
{
    param1 = getTrackbarPos("param1", "output");
}
void param2Call(int, void *)
{
    param2 = getTrackbarPos("param2", "output");
}
void minRadiusCall(int, void *)
{
    minRadius = getTrackbarPos("minRadius", "output");
}
void RadmaxiusCall(int, void *)
{
    Radmaxius = getTrackbarPos("Radmaxius", "output");
}
void channelCall(int, void *)
{
    channel = getTrackbarPos("channel", "merge");
}
void setTrackbarPosition(const string &trackbarName, const string &windowName, int initValue, void (*callback)(int, void *))
{
    createTrackbar(trackbarName, windowName, nullptr, MaxValue, callback);
    setTrackbarPos(trackbarName, windowName, initValue);
}
void initializeTrackbars()
{
    setTrackbarPosition("minDist", "output", DefaultMinDist, minDistCall);
    setTrackbarPosition("param1", "output", DefaultParam1, param1Call);
    setTrackbarPosition("param2", "output", DefaultParam2, param2Call);
    setTrackbarPosition("minRadius", "output", DefaultMinRadius, minRadiusCall);
    setTrackbarPosition("Radmaxius", "output", DefaultRadmaxius, RadmaxiusCall);
}

// -------------------------------------------------------------------------
// ---                    VOID about HoughtCircle                        ---
// -------------------------------------------------------------------------
void calibration(const Mat &input)
{
    FileStorage fs("..\\data\\calibration.xml", FileStorage::READ);

    Mat cameraMatrix, distCoeffs;
    fs["cameraMatrix"] >> cameraMatrix;
    fs["distCoeffs"] >> distCoeffs;

    undistort(input, dst, cameraMatrix, distCoeffs);
    fs.release();
}

// Custom comparator for sorting circles
bool compareCircles(const Vec3f &circle1, const Vec3f &circle2)
{
    // Sort by y-coordinate first (top to bottom)
    if (fabs(circle1[1] - circle2[1]) > 20)
    {
        return circle1[1] < circle2[1];
    }
    // If y-coordinates are the same, sort by x-coordinate (left to right)
    return circle1[0] < circle2[0];
}

void calcCircles(const Mat &input, vector<Vec3f> &circles)
{
    Mat temp, contours;
    Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
    // erode(input, temp, kernel);
    // imshow("erode 1",temp);
    // dilate(temp, temp, kernel);
    // imshow("dilate 1",temp);
    morphologyEx(input, temp, MORPH_OPEN, kernel);
    Canny(temp, contours, 50, 150);
    HoughCircles(contours, circles, HOUGH_GRADIENT, 1, minDist, param1, param2, minRadius, Radmaxius);
    sort(circles.begin(), circles.end(), compareCircles);
}

void drawCircle(Mat &input, const vector<Vec3f> &circles)
{
    for (int i = 0; i < circles.size(); i++)
    {
        Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
        int radius = cvRound(circles[i][2]);
        circle(input, center, radius, Scalar(0, 0, 255), 3, 8, 0);
    }
}

void detectCirclesAndDraw()
{
    calcCircles(dst, circles);
    cvtColor(dst, output, COLOR_GRAY2BGR);
    drawCircle(output, circles);
    putText(output, "Adjust trackbar to control Hough parameters,", Point(5, 20), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 255), 2);
    putText(output, "and press q or Ese exit window", Point(5, 50), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 255), 2);
    imshow("output", output);
}
// -------------------------------------------------------------------------
// ---                    VOID about circle detect                       --- binary threshold
// -------------------------------------------------------------------------
bool isCircleOutOfBounds(int centerX, int centerY, int radius, int maxWidth, int maxHeight)
{
    if (centerX - radius < 0 || centerY - radius < 0)
        return true;
    if (centerX + radius >= maxWidth || centerY + radius >= maxHeight)
        return true;
    return false;
}

void drawContoursOnImage(Mat &image, const vector<vector<Point>> &contours, double &defectSize)
{
    for (int i = 0; i < contours.size(); i++)
    {
        Rect rect = boundingRect(contours[i]);
        if ((rect.width > 5) || (rect.height > 5))
        {
            drawContours(image, contours, i, Scalar(0, 0, 255), 2);
            defectSize = defectSize + contourArea(contours[i]);
        }
    }
}

void processPolarCoordinates(const Mat &input, Mat &polarImg_Inv, int circleRadius)
{
    Point trans_center = Point(circleRadius, circleRadius);
    warpPolar(input, polarImg_Inv, Size(300, 600), trans_center, circleRadius, INTER_LINEAR | WARP_POLAR_LINEAR);
}

void circleDetect(double &defectSize)
{
    const bool isDebugMode = false;

    ROI = frame(Rect(circleCenterX - circleRadius, circleCenterY - circleRadius, 2 * circleRadius, 2 * circleRadius));

    // Process polar coordinates
    processPolarCoordinates(ROI, dst, circleRadius);
    // Process blur and difference
    Mat blurredDst, diff, diffLow, binary, grayDst;
    cvtColor(dst, grayDst, COLOR_RGB2GRAY);
    blur(grayDst, blurredDst, Size(3, 501), Point(-1, -1));
    absdiff(grayDst, blurredDst, diff);
    cvtColor(diff, grayDiff, COLOR_GRAY2RGB);
    // kill hight value
    // threshold(diff, diffLow, brightness, 255, THRESH_TOZERO_INV); // 115
    // threshold(diff, binary, 70, 255, THRESH_BINARY);
    threshold(diff, binary, 70, 255, THRESH_BINARY);
    medianBlur(binary, binary, 3);

    // Find contours and draw defects
    vector<vector<Point>> contours;
    findContours(binary, contours, RETR_EXTERNAL, CHAIN_APPROX_NONE, Point());
    drawContoursOnImage(grayDiff, contours, defectSize);

    // Inverse to circle
    warpPolar(grayDiff, polarImg_Inv, ROI.size(), Point(circleRadius, circleRadius), circleRadius, INTER_LINEAR | WARP_POLAR_LINEAR | WARP_INVERSE_MAP);
    circle(polarImg_Inv, Point(circleRadius, circleRadius), 3, Scalar(0, 255, 0), -1, 8, 0);
    circle(polarImg_Inv, Point(circleRadius, circleRadius), circleRadius, Scalar(255, 0, 0), 3, 8, 0);

    if (isDebugMode)
    {
        // imshow("Region of Interest", ROI);
        // imshow("Polar Coordinates", dst);
        // imshow("Gray Image", grayDst);
        // imshow("Blurred Image", blurredDst);
        imshow("Difference", diff);
        imshow("hight kill", diffLow);
        imshow("Binary Image", binary);
        // imshow("Inverse Polar", polarImg_Inv);
    }
}

// -------------------------------------------------------------------------
// ---                   VOID about image merge                          --- defectSize
// -------------------------------------------------------------------------
void copyImageRegion(Mat &src, Mat &dest, Rect sourceRect, Rect destRect)
{
    Mat srcClone = src.clone();
    srcClone.copyTo(dest(destRect));
}

void drawNGText(Mat &image, int centerX, int centerY)
{
    Point center(centerX, centerY);
    putText(image, "NG", center, FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 0, 255), 2);
}

void drawCirclesAndText(Mat &image, int centerX, int centerY, int radius, double &defectSize)
{
    Point center(centerX + 20, centerY + 20);
    circle(image, center, 3, Scalar(0, 255, 0), -1, 8, 0);
    circle(image, center, radius, Scalar(255, 0, 0), 3, 8, 0);

    int outsetX = frame.cols;
    putText(image, "Adjust channel trackbar to control image show, and click in the polar coordinate area to control brightness threshold.", Point(5, 20), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 255), 2);
    putText(image, "Press q or Ese exit window", Point(5, 50), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 255), 2);
    putText(image, "src :", Point(10, 90), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
    putText(image, "ROI :", Point(outsetX + 10, 90), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
    putText(image, "defect :", Point(outsetX + 10, 340), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
    putText(image, "polar coordinates :", Point(frame.cols + ROI.cols + polarImg_Inv.cols, 90), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
    putText(image, "radius = " + to_string(radius), Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.75, Scalar(255, 0, 0), 2);
    putText(image, "defectSize = " + to_string(lround(defectSize)), Point(30, 150), FONT_HERSHEY_SIMPLEX, 0.75, Scalar(255, 0, 0), 2);
}

bool hasDefect(double &defectSize)
{
    if (defectSize > 2000.0) // TODO:
        return true;
    else
        return false;
}

void imgMerge(Mat &srcClone, double &defectSize)
{
    int padding = 20;

    int width = frame.cols + ROI.cols + dst.cols + polarImg_Inv.cols + padding * 4;
    int height = max(max(ROI.rows, dst.rows), polarImg_Inv.rows) + padding * 2;
    mergeIMG = Mat3b(height, width, Vec3b(0, 0, 0));

    if (hasDefect(defectSize))
    {
        drawNGText(srcClone, circleCenterX, circleCenterY);
    }
    // create sections
    Rect r0(padding, padding, srcClone.cols, srcClone.rows);
    Rect r1(frame.cols + padding * 2, padding + 70, ROI.cols, ROI.rows);
    Rect r2(frame.cols + padding * 2, padding + 360, polarImg_Inv.cols, polarImg_Inv.rows);
    Rect r3(frame.cols + ROI.cols + polarImg_Inv.cols + padding * 3, padding, dst.cols, dst.rows);

    // copy images
    copyImageRegion(srcClone, mergeIMG, Rect(0, 0, frame.cols, frame.rows), r0);
    copyImageRegion(ROI, mergeIMG, Rect(0, 0, ROI.cols, ROI.rows), r1);
    copyImageRegion(polarImg_Inv, mergeIMG, Rect(0, 0, polarImg_Inv.cols, polarImg_Inv.rows), r2);
    copyImageRegion(grayDiff, mergeIMG, Rect(0, 0, dst.cols, dst.rows), r3); // grayDiff

    drawCirclesAndText(mergeIMG, circleCenterX, circleCenterY, circleRadius, defectSize);

    imshow("merge", mergeIMG);
}