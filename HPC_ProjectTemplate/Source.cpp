#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <msclr\marshal_cppstd.h>
#include <ctime>// include this header 
#pragma once

#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>
using namespace std;
using namespace msclr::interop;

int* inputImage(int* w, int* h, System::String^ imagePath) //put the size of image in w & h
{
	int* input;


	int OriginalImageWidth, OriginalImageHeight;

	//*********************************************************Read Image and save it to local arrayss*************************	
	//Read Image and save it to local arrayss

	System::Drawing::Bitmap BM(imagePath);

	OriginalImageWidth = BM.Width;
	OriginalImageHeight = BM.Height;
	*w = BM.Width;
	*h = BM.Height;
	int* Red = new int[BM.Height * BM.Width];
	int* Green = new int[BM.Height * BM.Width];
	int* Blue = new int[BM.Height * BM.Width];
	input = new int[BM.Height * BM.Width];
	for (int i = 0; i < BM.Height; i++)
	{
		for (int j = 0; j < BM.Width; j++)
		{
			System::Drawing::Color c = BM.GetPixel(j, i);

			Red[i * BM.Width + j] = c.R;
			Blue[i * BM.Width + j] = c.B;
			Green[i * BM.Width + j] = c.G;

			input[i * BM.Width + j] = ((c.R + c.B + c.G) / 3); //gray scale value equals the average of RGB values

		}

	}
	return input;
}


void createImage(int* image, int width, int height, int index)
{
	System::Drawing::Bitmap MyNewImage(width, height);


	for (int i = 0; i < MyNewImage.Height; i++)
	{
		for (int j = 0; j < MyNewImage.Width; j++)
		{
			//i * OriginalImageWidth + j
			if (image[i * width + j] < 0)
			{
				image[i * width + j] = 0;
			}
			if (image[i * width + j] > 255)
			{
				image[i * width + j] = 255;
			}
			System::Drawing::Color c = System::Drawing::Color::FromArgb(image[i * MyNewImage.Width + j], image[i * MyNewImage.Width + j], image[i * MyNewImage.Width + j]);
			MyNewImage.SetPixel(j, i, c);
		}
	}
	MyNewImage.Save("..//Data//Output//outputRes" + index + ".png");
	cout << "result Image Saved " << index << endl;
}


int main()
{
	int ImageWidth = 4, ImageHeight = 4;

	int start_s, stop_s, TotalTime = 0;

	System::String^ imagePath;
	std::string img;
	img = "..//Data//Input//test.png";

	imagePath = marshal_as<System::String^>(img);
	int* imageData = inputImage(&ImageWidth, &ImageHeight, imagePath);

	start_s = clock();


	MPI_Init(NULL, NULL);
	int rank, size;

	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	int* local_imgData = new int[(ImageWidth * ImageHeight) / size];

	int* num_of_pixels = new int[256];

	int* rec_totalNumOfPixels = new int[256];
	int total_numOfPixels[256] = { 0 };

	for (int i = 0; i < 256; i++) {
		num_of_pixels[i] = 0;
	}
	
	for (int i = 0; i < 256; i++) {
		rec_totalNumOfPixels[i] = 0;
	}

	for (int i = 0; i < 256; i++) {
		total_numOfPixels[i] = 0;
	}

	
	
	double* local_commulative = new double[256];

	double global_probability[256] = { 0 };
	
	double* local_probability = new double[256 / size];
	
	double NewglobalTotalSum = 0;

	double commulative_probability[256] = { 0 };
	int result_image[256] = { 0 };

	int* local_floor = new int[256 / size];

	MPI_Scatter(imageData, (ImageWidth * ImageHeight) / size, MPI_INT, local_imgData, (ImageWidth * ImageHeight) / size, MPI_INT, 0, MPI_COMM_WORLD);

	for (int i = 0; i < (ImageWidth * ImageHeight) / size; i++)
		num_of_pixels[local_imgData[i]]++;


	MPI_Reduce(num_of_pixels, &total_numOfPixels, 256, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	

	// probability
	MPI_Scatter(&total_numOfPixels, 256 / size, MPI_INT, rec_totalNumOfPixels, 256 / size, MPI_INT, 0, MPI_COMM_WORLD);

	

	for (int i = 0; i < 256 / size; i++)
		local_probability[i] = (double)((double)rec_totalNumOfPixels[i] / (double)(ImageWidth*ImageHeight));

	MPI_Gather(local_probability, 256 / size, MPI_DOUBLE, &global_probability, 256 / size, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	
	// commulative 
	if (rank == 0)
	{
		commulative_probability[0] = global_probability[0];
		for (int i = 1; i < 256; i++)
			commulative_probability[i] = global_probability[i] + commulative_probability[i - 1];
	}

	// flooring and scalling
	MPI_Scatter(&commulative_probability, 256 / size, MPI_DOUBLE, local_commulative, 256 / size, MPI_DOUBLE, 0, MPI_COMM_WORLD);

	for (int i = 0; i < 256 / size; i++)
	{
		local_probability[i] = local_commulative[i] * 256 ;
		local_floor[i] = floor(local_probability[i]);
	}


	MPI_Gather(local_floor, 256 / size, MPI_INT, result_image, 256 / size, MPI_INT, 0, MPI_COMM_WORLD);

	if (rank == 0)
	{

		for (int i = 0; i < ImageWidth * ImageHeight; i++)
			for (int j = 0; j < 256; j++) {
				if (j == imageData[i]) {
					imageData[i] = result_image[j];
					break;
				}
			}
		createImage(imageData, ImageWidth, ImageHeight, 1);
	}

	MPI_Finalize();
	stop_s = clock();
	TotalTime += (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;
	cout << "time: " << TotalTime << endl;

	free(imageData);
	
	return 0;

}
