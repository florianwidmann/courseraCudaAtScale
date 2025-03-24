# Rotated Thumbnail Creation using NVIDIA NPP with CUDA

## Overview

This little command line application takes input images as command line arguments and produces (quadratic) thumbnails where the image has been rotated by 45 degrees (with other parts of the thumbnail being black). The application uses the NVIDIA Performance Primitives (NPP) library with CUDA, so it requires a NVIDIA GPU to run. It is a toy application in the context of a CUDA training course and hence not overly useful in real life.

## Code Organization

```bin/```
This folder will contain the executable (if the provided Makefile is used).

```lib/```
This folders containes some NPP related (header only) helper libraries which are used by the main code.

```src/```
This folder contains the source code of the main application.

```Makefile```
A simple Makefile which can either be used directly or serve as "inspiration".

## Dependencies and Prerequisites

The libraries FreeImage and NPP and the CUDA toolkit are required to build and run the application. A NVIDIA GPU is required for running the application.

The library was tested on Linux and x86_64 but doesn't use anything platform or OS specific.
Hence, it might also work on other platforms and operating systems.

The application should understand most common image file types. The exact support depends on the system installation.

## Building the Application

A simple Makefile is provided, try "make help" regarding its usage.
If the Makefile cannot be used directly, it should still give some inspiration for a manual build.

## Running the Application

A successful build will create a single command line based executable (in ```bin/``` if the provided Makefile is used).
Try invoking the executable with the flag "--help" for more information.
