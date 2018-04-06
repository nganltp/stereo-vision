#include "api_kinect_cv.h"

int
api_kinect_cv_get_images(VideoCapture &capture,
        Mat &depthMap,
        Mat &grayImage)
{
    if( !capture.isOpened() )
    {
        cout << "Can not open a capture object." << endl;
        return -1;
    }

    if( !capture.grab() )
    {
        cout << "Can not grab images." << endl;
        return -1;
    }
    else
    {
        Mat depth;
        if( capture.retrieve( depth, CV_CAP_OPENNI_DEPTH_MAP ) )
        {
            //depthMap = depth.clone();
            const float scaleFactor = 0.05f;
            depth.convertTo( depthMap, CV_8UC1, scaleFactor );
        }
        else
        {
            cout<< endl<< "Error: Cannot get depthMap image";
            return -1;
        }

        if( !capture.retrieve( grayImage, CV_CAP_OPENNI_GRAY_IMAGE ) )
        {
            cout<< endl<< "Error: Cannot get gray image";
            return -1;
        }
    }

    return 0;
}


void
mergeOverlappingBoxes(std::vector<cv::Rect> &inputBoxes,
                      cv::Mat &image,
                      std::vector<cv::Rect> &outputBoxes)
{
    cv::Mat mask = cv::Mat::zeros(image.size(), CV_8UC1); // Mask of original image
    cv::Size scaleFactor(15,15);

    for (int i = 0; i < inputBoxes.size(); i++)
    {
        cv::Rect box = inputBoxes.at(i) + scaleFactor;
        cv::rectangle(mask, box, cv::Scalar(255), CV_FILLED); // Draw filled bounding boxes on mask
    }

    std::vector< std::vector< cv::Point> > contours;
    // Find contours in mask
    // If bounding boxes overlap, they will be joined by this function call
    cv::findContours(mask, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);
    for (int j = 0; j < contours.size(); j++)
    {
        outputBoxes.push_back(cv::boundingRect(contours.at(j)));
    }
}


void
truncate(Mat &src, Mat &dst,
         int lower_bound, int upper_bound)
{
    for(int y = 0; y < src.rows; y++)
    {
        for(int x = 0; x < src.cols; x++)
        {
            uchar &d = src.at<uchar>(y,x);
            uchar &e = dst.at<uchar>(y,x);

            if( d > lower_bound && d < upper_bound )
                e = 255;
            else
                e = 0;
        }
    }
}


void
api_kinect_cv_get_obtacle_rect( Mat& depthMap,
                                vector< Rect > &output_boxes,
                                Rect &roi,
                                int lower_bound,
                                int upper_bound
                           )
{

    int thresh_area = 200;

    int erosion_type = MORPH_ELLIPSE;
    int erosion_size = 1;


    Mat element = getStructuringElement( erosion_type,
                                         Size( 2*erosion_size + 1, 2*erosion_size+1 ),
                                         Point( erosion_size, erosion_size ) );

    Mat tmpDepthMap = depthMap(roi).clone();
    Mat binImg = depthMap(roi).clone();

    truncate( tmpDepthMap, binImg, lower_bound, upper_bound);

    erode ( binImg, binImg, element );
    dilate( binImg, binImg, element );

//    imshow( "depthMap", tmpDepthMap );
//    imshow( "depthMapBin", binImg );

    vector<vector<Point> > contours;
    vector<Vec4i> hierarchy;

    /// Find contours
    findContours( binImg, contours, hierarchy, CV_RETR_EXTERNAL,
                  CV_CHAIN_APPROX_SIMPLE, Point(0, 0) );

    /// Approximate contours to polygons + get bounding rects and circles
    vector<vector<Point> > contours_poly( contours.size() );
    vector<Rect> boundRect1( contours.size() );
    vector<Rect> boundRect2;

    for( int i = 0; i < contours.size(); i++ )
    {
        approxPolyDP( Mat(contours[i]), contours_poly[i], 3, true );
        boundRect1[i] = boundingRect( Mat(contours_poly[i]) );
    }

    Mat binImg1 = Mat::zeros(depthMap.size(), CV_8UC1);
    Mat binImg2 = Mat::zeros(depthMap.size(), CV_8UC1);

    for( int i = 0; i< contours.size(); i++ )
    {
        if( boundRect1[i].area() > thresh_area )
            boundRect2.push_back(boundRect1[i]);
    }

    for( int i = 0; i< boundRect2.size(); i++ )
    {
        rectangle( binImg1, boundRect2[i], Scalar( 255), CV_FILLED);
    }

//    imshow( "depthMapBin1", binImg1 );

    vector<Rect> tmp_outputBoxes;

    mergeOverlappingBoxes( boundRect2, binImg2, tmp_outputBoxes);

    for( int i = 0; i< tmp_outputBoxes.size(); i++ )
    {
        if( tmp_outputBoxes[i].area() > 1.5*thresh_area )
            output_boxes.push_back( tmp_outputBoxes[i] + roi.tl());
    }
}
/*
void
api_kinect_cv_center_rect_gen(
        vector< Rect > &rects,
        int frame_width,
        int frame_height
        )
{
    int start_y = 140;
    int end_y   = 320;
    int x_max = 200;
    int x_min = 160;
    for( int i = 0; i < 16; i++ )
    {
        int x, y, w, h;
        x = frame_width / 2 - x_max/2 + i*5;
        w = x_max - i*10;
        y = frame_height - start_y - i*10;
        h = 10;
        Rect roi( x, y, w, h);
        rects.push_back( roi );
    }
}
*/
void get_obtacle(Mat depthMap, int &xL, int &xR, int &y, int &w, int &h)
{
     // Init//
    int slice_nb = 3;
    int lower_slice_idx = 3;
    int upper_slice_idx = lower_slice_idx + slice_nb;
    //int lower_bound = DIST_MIN + lower_slice_idx * SLICE_DEPTH;
    //int upper_bound = lower_bound + slice_nb*SLICE_DEPTH;
    int lower_bound = 30;
    int upper_bound = 50;

    Mat depthMap;
    Mat grayImage;
    vector< Rect > rects;
    Rect intersect;
    vector< Rect > output_boxes;

    Rect roi_1 = Rect(0, VIDEO_FRAME_HEIGHT/4,
                    VIDEO_FRAME_WIDTH, VIDEO_FRAME_HEIGHT/2);
    Rect roi_2(0, 132, 640, 238);

    int frame_width = capture.get( CV_CAP_PROP_FRAME_WIDTH );
    int frame_height = capture.get( CV_CAP_PROP_FRAME_HEIGHT );

    //api_kinect_cv_get_images(capture, depthMap , grayImage); //get depth

    crop_grayImage = grayImage(roi_1);
    crop_depthMap = depthMap(roi_1);

    api_kinect_cv_get_obtacle_rect( depthMap, output_boxes, roi_2, lower_bound, upper_bound );
    Mat binImg = Mat::zeros(depthMap.size(), CV_8UC1);

    api_kinect_cv_center_rect_gen( rects, frame_width, frame_height);
    Rect center_rect = rects[lower_slice_idx];
    center_rect = center_rect + Size(0, slice_nb*SLICE_DEPTH); // lay center_rect

    for( int i = 0; i< output_boxes.size(); i++ )
        {
//                    rectangle( binImg, output_boxes[i], Scalar( 255) );
            //intersect = output_boxes[i] & center_rect;
            intersect = output_boxes[i];
            if( intersect.area() != 0 )
               {
                    rectangle( binImg, intersect, Scalar( 255) );
                    //cout<< endl<< "Has Collision detect"<< flush;
                }
        }
        //lay toa do, rong, cao.
    xL = intersect.x;
    xR = intersect.x + intersect.width;
    y = intersect.y;
    w = intersect.width;
    h = intersect.height;

    imshow( "BoundingRect", binImg );
    if(!grayImage.empty())
        imshow( "gray", crop_grayImage );

    if (!depthMap.empty() )
        imshow( "depth", crop_depthMap );
}
void PointCenter_Displacement(int &xTam, int xL, int xR)
{
    int dodaixe = 30;
    int distan = 7;

    if (xTam < xL)
    {
        if (  xTam > (xL - dodaixe/2 - distan) )
            xTam = xL + dodaixe/2 + distan;
        else
            xTam = xTam;
    }
    else if (xTam > xR)
    {
        if (  xTam < (xR + dodaixe/2 + distan) )
            xTam = xL + dodaixe/2 + distan;
        else
            xTam = xTam;
    }
    else
    {

    }
}
