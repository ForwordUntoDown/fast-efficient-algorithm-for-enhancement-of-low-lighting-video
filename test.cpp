#include <iostream>  
#include <opencv2/core/core.hpp>  
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <vector>
#include <algorithm>

using namespace std;
using namespace cv;

//���һ�����������Ԫ��
void print_arr(int arr[], int len)
{
	for (int i = 0; i < len; i++)
		cout << arr[i] << " ";
	cout << endl;
}

//��ͼ����з�ɫ����
void get_inverted_img(Mat img, Mat inverted)
{
	for (int row = 0; row < inverted.rows; row++)
		for (int col = 0; col < inverted.cols; col++)
			for (int channel = 0; channel < inverted.channels(); channel++)
				inverted.at<Vec3b>(row, col)[channel] = 255 - img.at<Vec3b>(row, col)[channel];
}

//���ݷ�ɫͼ��ò���A��ֵ��R,G,B��Ӧ��������ֵ��
void get_A_value(Mat M, int A[])
{
	//MM���Mat��ŵ���M�У�ÿ������ֵR,G,B����ͨ���е���С��һ����������һά�ġ�
	Mat MM = Mat::zeros(M.rows, M.cols, CV_8UC1);
	for (int row = 0; row < M.rows; row++)
	{
		for (int col = 0; col < M.cols; col++)
		{
			int min_intensity = 255;
			for (int channel = 0; channel < M.channels(); channel++)
				if (min_intensity > M.at<Vec3b>(row, col)[channel])
					min_intensity = M.at<Vec3b>(row, col)[channel];
			MM.at<uchar>(row, col) = min_intensity;
		}
	}

	//top_k��ŵ���MM���Mat������ǰ100��������Ӧ��λ�ã�����ֵת����һ����������
	int top_k[100] = { 0 };
	int len = 100;
	int max_value = 0;
	int index = -1;

	for (int i = 0; i < len; i++)
	{
		max_value = 0;
		index = -1;
		for (int row = 0; row < MM.rows; row++)
		{
			for (int col = 0; col < MM.cols; col++)
			{
				if (MM.at<uchar>(row, col) > max_value)
				{
					max_value = MM.at<uchar>(row, col);
					index = row * MM.cols + col;
				}
			}
		}
		top_k[i] = index;
		MM.at<uchar>(index / MM.cols, index % MM.cols) = 0;
	}

	//�ҳ���һ�ٸ�������RGB�ܺ������Ǹ�����
	max_value = 0;
	index = -1;
	int sum = 0;
	for (int i = 0; i < len; i++)
	{
		sum = 0;
		for (int channel = 0; channel < M.channels(); channel++)
			sum = sum + M.at<Vec3b>(top_k[i] / M.cols, top_k[i] % M.cols)[channel];
		if (sum >= max_value)
		{
			max_value = sum;
			index = i;
		}
	}

	//�޸�ȫ�����ֵA
	for (int channel = 0; channel < M.channels(); channel++)
		A[channel] = M.at<Vec3b>(top_k[index] / M.cols, top_k[index] % M.cols)[channel];
	return;
}

//��M[row][col][channel]Ϊ���ģ��ߴ�Ϊkernel_size����Сֵ�˲����
int minimum_filter(Mat M, int row, int col, int kernel_size, int channel)
{
	int temp = 255;
	int offset = (kernel_size - 1) / 2;

	for (int i = (row - offset); i <= (row + offset); i++)
		for (int j = (col - offset); j <= (col + offset); j++)
			if (M.at<Vec3b>(i, j)[channel] < temp)
				temp = M.at<Vec3b>(i, j)[channel];
	return temp;
}

//����ÿ������ֵ������t(x)��ֵ
void t_for_each_pixel(Mat T, Mat R, int A[])
{
	//��R�����˲�ʱ���Ƚ���padding����Сֵ�˲���˲�255��padding֮��洢��RR��
	//(�������˲����ĳߴ�Ϊ9)

	double omega_param = 0.8;
	int kernel_size = 9;
	int offset = (kernel_size - 1) / 2;

	Mat RR(R.rows + 2 * offset, R.cols + 2 * offset, CV_8UC3, Scalar(255, 255, 255));

	for (int row = 0; row < R.rows; row++)
		for (int col = 0; col < R.cols; col++)
			for (int channel = 0; channel < R.channels(); channel++)
				RR.at<Vec3b>(row + offset, col + offset)[channel] = R.at<Vec3b>(row, col)[channel];

	for (int row = offset; row < (R.rows + offset); row++)
	{
		for (int col = offset; col < (R.cols + offset); col++)
		{
			double temp = 255.0;
			for (int channel = 0; channel < RR.channels(); channel++)
			{
				double filtered_val = (double)minimum_filter(RR, row, col, kernel_size, channel)*1.0;
				filtered_val = filtered_val / ((double)A[channel]);

				if (filtered_val < temp)
					temp = filtered_val;
			}
			T.at<double>(row - offset, col - offset) = 1.0 - omega_param *temp;
		}
	}
	return;
}

void recovery_img(Mat J, Mat R, Mat T, int A[])
{
	for (int row = 0; row < J.rows; row++)
	{
		for (int col = 0; col < J.cols; col++)
		{
			double p = 0;
			if (T.at<double>(row, col) < 0.5 && T.at<double>(row, col) > 0)
				p = 2 * T.at<double>(row, col);
			else
				p = 1;
			for (int channel = 0; channel < J.channels(); channel++)
			{
				double temp = ((double)(R.at<Vec3b>(row, col)[channel]) - A[channel]) / (p*T.at<double>(row, col));
				temp = temp + A[channel];
				J.at<Vec3b>(row, col)[channel] = (uchar)temp;
			}
		}
	}
	return;
}

//�����˲���
Mat guidedfilter(Mat &srcImage, Mat &srcClone, int r, double eps)
{
	//ת��Դͼ����Ϣ	
	srcImage.convertTo(srcImage, CV_64FC1);
	srcClone.convertTo(srcClone, CV_64FC1);
	int nRows = srcImage.rows;
	int nCols = srcImage.cols;
	Mat boxResult;

	//����һ�������ֵ	
	boxFilter(Mat::ones(nRows, nCols, srcImage.type()), boxResult, CV_64FC1, Size(r, r));

	//���ɵ����ֵmean_I	
	Mat mean_I;
	boxFilter(srcImage, mean_I, CV_64FC1, Size(r, r));

	//����ԭʼ��ֵmean_p	
	Mat mean_p;
	boxFilter(srcClone, mean_p, CV_64FC1, Size(r, r));

	//���ɻ���ؾ�ֵmean_Ip	
	Mat mean_Ip;
	boxFilter(srcImage.mul(srcClone), mean_Ip, CV_64FC1, Size(r, r));
	Mat cov_Ip = mean_Ip - mean_I.mul(mean_p);

	//��������ؾ�ֵmean_II	
	Mat mean_II;

	//Ӧ�ú��˲���������ص�ֵ	
	boxFilter(srcImage.mul(srcImage), mean_II, CV_64FC1, Size(r, r));

	//��������������ϵ��	
	Mat var_I = mean_II - mean_I.mul(mean_I);
	Mat var_Ip = mean_Ip - mean_I.mul(mean_p);

	//���������������ϵ��a,b	
	Mat a = cov_Ip / (var_I + eps);
	Mat b = mean_p - a.mul(mean_I);

	//�����ģ�����ϵ��a\b�ľ�ֵ	
	Mat mean_a;	
	boxFilter(a, mean_a, CV_64FC1, Size(r, r));
	mean_a = mean_a / boxResult;
	Mat mean_b;	
	boxFilter(b, mean_b, CV_64FC1, Size(r, r));
	mean_b = mean_b / boxResult;

	//�����壺�����������	
	Mat resultMat = mean_a.mul(srcImage) + mean_b;
	return resultMat;
}

//���������˲�����
void filter(Mat M, int radius)
{
	vector<Mat> vSrcImage, vResultImage;
	split(M, vSrcImage);
	
	Mat resultMat;
	for (int i = 0; i < 3; i++)
	{
		//��ͨ��ת���ɸ�����		
		Mat tempImage;
		vSrcImage[i].convertTo(tempImage, CV_64FC1, 1.0 / 255.0);
		Mat p = tempImage.clone();

		//�ֱ���е����˲�		
		Mat resultImage = guidedfilter(tempImage, p, radius, 0.01);
		vResultImage.push_back(resultImage);
	}
	//ͨ������ϲ�	
	merge(vResultImage, resultMat);
	resultMat.convertTo(resultMat, CV_8UC1, 255, 0);

	imshow("result", resultMat);
	imwrite("result.bmp", resultMat);
}

int main()
{
	Mat origion = imread("test_1.bmp");
	if (!origion.data)
	{
		cout << "����ͼ��ʧ��" << endl;
		return 0;
	}
	cout << "����ͼ��ĳߴ�Ϊ��" << origion.rows << " " << origion.cols << endl;
	namedWindow("origion", CV_WINDOW_AUTOSIZE);
	imshow("origion", origion);

	//R����ɫ֮���ͼ
	Mat R = Mat::zeros(origion.rows, origion.cols, CV_8UC3);
	get_inverted_img(origion, R);

	//A������global atmosphere light
	int A[] = { 0,0,0 };
	get_A_value(R, A);
	cout << "A��ֵΪ(opencv:b,g,r˳��)" << " ";
	print_arr(A, sizeof(A) / sizeof(int));

	//����ÿһ�����ص�x������t(x)��ֵ��T������Ϊdouble
	Mat T = Mat::zeros(origion.rows, origion.cols, CV_64FC1);
	t_for_each_pixel(T, R, A);

	//�ɷ�ɫͼR��T���Լ�A���õ�ȥ����ͼJ
	Mat J = Mat::zeros(origion.rows, origion.cols, CV_8UC3);
	recovery_img(J, R, T, A);

	//��J�ٴη�ɫ���õ����յ����ͼE
	Mat E = Mat::zeros(origion.rows, origion.cols, CV_8UC3);
	get_inverted_img(J, E);
	imwrite("enhancement_without_guidedfilter.bmp", E);

	//�����յ����ͼ���е����˲����õ���ǿ��Ľ��E
	int radius = 11;
	filter(E, radius);

	cvWaitKey(0);
	return 0;
}
