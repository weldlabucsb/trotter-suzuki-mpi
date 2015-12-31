/**
 * Distributed Trotter-Suzuki solver
 * Copyright (C) 2015 Luca Calderaro, 2012-2015 Peter Wittek,
 * 2010-2012 Carlos Bederián
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <cassert>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <cmath>
#include <stdlib.h>
#include <string.h>

#if HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_MPI
#include <mpi.h>
#endif

#include "common.h"

void calculate_borders(int coord, int dim, int * start, int *end, int *inner_start, int *inner_end, int length, int halo, int periodic_bound) {
    int inner = (int)ceil((double)length / (double)dim);
    *inner_start = coord * inner;
    if(periodic_bound != 0)
        *start = *inner_start - halo;
    else
        *start = ( coord == 0 ? 0 : *inner_start - halo );
    *end = *inner_start + (inner + halo);

    if (*end > length) {
        if(periodic_bound != 0)
            *end = length + halo;
        else
            *end = length;
    }
    if(periodic_bound != 0)
        *inner_end = *end - halo;
    else
        *inner_end = ( *end == length ? *end : *end - halo );
}

void read_potential(double * external_pot_real, double * external_pot_imag, int tile_width, int tile_height,
                    char * pot_name, int matrix_width, int matrix_height, int start_x, int start_y,
                    int * periods, int * coords, int * dims, int halo_x, int halo_y, double time_single_it, double particle_mass, bool imag_time) {
    std::ifstream input(pot_name);

    int in_width = matrix_width - 2 * periods[1] * halo_x;
    int in_height = matrix_height - 2 * periods[0] * halo_y;
    std::complex<double> tmp;
    double read;
    double order_approx = 2.;
    double CONST_1 = -1. * time_single_it * order_approx;
    double CONST_2 = 2. * time_single_it / particle_mass * order_approx;		//CONST_2: discretization of momentum operator and the only effect is to produce a scalar operator, so it could be omitted

    for(int i = 0; i < in_height; i++) {
        for(int j = 0; j < in_width; j++) {
            input >> read;
            if(imag_time)
                tmp = exp(std::complex<double> (CONST_1 * read , CONST_2));
            else
                tmp = exp(std::complex<double> (0., CONST_1 * read + CONST_2));

            if((i - start_y) >= 0 && (i - start_y) < tile_height && (j - start_x) >= 0 && (j - start_x) < tile_width) {
                external_pot_real[(i - start_y) * tile_width + j - start_x] = real(tmp);
                external_pot_imag[(i - start_y) * tile_width + j - start_x] = imag(tmp);
            }

            //Down band
            if(i < halo_y && coords[0] == dims[0] - 1 && periods[0] != 0) {
                if((j - start_x) >= 0 && (j - start_x) < tile_width) {
                    external_pot_real[(i + tile_height - halo_y) * tile_width + j - start_x] = real(tmp);
                    external_pot_imag[(i + tile_height - halo_y) * tile_width + j - start_x] = imag(tmp);
                }
                //Down right corner
                if(j < halo_x && periods[1] != 0 && coords[1] == dims[1] - 1) {
                    external_pot_real[(i + tile_height - halo_y) * tile_width + j + tile_width - halo_x] = real(tmp);
                    external_pot_imag[(i + tile_height - halo_y) * tile_width + j + tile_width - halo_x] = imag(tmp);
                }
                //Down left corner
                if(j >= in_width - halo_x && periods[1] != 0 && coords[1] == 0) {
                    external_pot_real[(i + tile_height - halo_y) * tile_width + j - (in_width - halo_x)] = real(tmp);
                    external_pot_imag[(i + tile_height - halo_y) * tile_width + j - (in_width - halo_x)] = imag(tmp);
                }
            }

            //Upper band
            if(i >= in_height - halo_y && periods[0] != 0 && coords[0] == 0) {
                if((j - start_x) >= 0 && (j - start_x) < tile_width) {
                    external_pot_real[(i - (in_height - halo_y)) * tile_width + j - start_x] = real(tmp);
                    external_pot_imag[(i - (in_height - halo_y)) * tile_width + j - start_x] = imag(tmp);
                }
                //Up right corner
                if(j < halo_x && periods[1] != 0 && coords[1] == dims[1] - 1) {
                    external_pot_real[(i - (in_height - halo_y)) * tile_width + j + tile_width - halo_x] = real(tmp);
                    external_pot_imag[(i - (in_height - halo_y)) * tile_width + j + tile_width - halo_x] = imag(tmp);
                }
                //Up left corner
                if(j >= in_width - halo_x && periods[1] != 0 && coords[1] == 0) {
                    external_pot_real[(i - (in_height - halo_y)) * tile_width + j - (in_width - halo_x)] = real(tmp);
                    external_pot_imag[(i - (in_height - halo_y)) * tile_width + j - (in_width - halo_x)] = imag(tmp);
                }
            }

            //Right band
            if(j < halo_x && periods[1] != 0 && coords[1] == dims[1] - 1) {
                if((i - start_y) >= 0 && (i - start_y) < tile_height) {
                    external_pot_real[(i - start_y) * tile_width + j + tile_width - halo_x] = real(tmp);
                    external_pot_imag[(i - start_y) * tile_width + j + tile_width - halo_x] = imag(tmp);
                }
            }

            //Left band
            if(j >= in_width - halo_x && periods[1] != 0 && coords[1] == 0) {
                if((i - start_y) >= 0 && (i - start_y) < tile_height) {
                    external_pot_real[(i - start_y) * tile_width + j - (in_width - halo_x)] = real(tmp);
                    external_pot_imag[(i - start_y) * tile_width + j - (in_width - halo_x)] = imag(tmp);
                }
            }
        }
    }
    input.close();
}

double const_potential(int x, int y, Lattice *grid) {
    return 0.;
}

void initialize_exp_potential(Lattice *grid, double * external_pot_real, double * external_pot_imag, double (*hamilt_pot)(int x, int y, Lattice *grid),
                              double time_single_it, double particle_mass, bool imag_time) {
      double order_approx = 2.;
      double CONST_1 = -1. * time_single_it * order_approx;
      double CONST_2 = 2. * time_single_it / particle_mass * order_approx;		//CONST_2: discretization of momentum operator and the only effect is to produce a scalar operator, so it could be omitted

      std::complex<double> tmp;
      for (int y = 0, idy = grid->start_y; y < grid->dim_y; y++, idy++) {
          for (int x = 0, idx = grid->start_x; x < grid->dim_x; x++, idx++) {
              if(imag_time)
                  tmp = exp(std::complex<double> (CONST_1 * hamilt_pot(idx, idy, grid) , CONST_2));
              else
                  tmp = exp(std::complex<double> (0., CONST_1 * hamilt_pot(idx, idy, grid) + CONST_2));

              external_pot_real[y * grid->dim_x + x] = real(tmp);
              external_pot_imag[y * grid->dim_x + x] = imag(tmp);
          }
      }
}

void print_complex_matrix(char * filename, double * matrix_real, double * matrix_imag, size_t stride, size_t width, size_t height) {
    std::ofstream out(filename, std::ios::out | std::ios::trunc);
    for (size_t i = 0; i < height; ++i) {
        for (size_t j = 0; j < width; ++j) {
            out << "(" << matrix_real[i * stride + j] << "," << matrix_imag[i * stride + j] << ") ";
        }
        out << std::endl;
    }
    out.close();
}

void print_matrix(const char * filename, double * matrix, size_t stride, size_t width, size_t height) {
    std::ofstream out(filename, std::ios::out | std::ios::trunc);
    for (size_t i = 0; i < height; ++i) {
        for (size_t j = 0; j < width; ++j) {
            out << matrix[i * stride + j] << " ";
        }
        out << std::endl;
    }
    out.close();
}

void memcpy2D(void * dst, size_t dstride, const void * src, size_t sstride, size_t width, size_t height) {
    char *d = reinterpret_cast<char *>(dst);
    const char *s = reinterpret_cast<const char *>(src);
    for (size_t i = 0; i < height; ++i) {
        for (size_t j = 0; j < width; ++j) {
            d[i * dstride + j] = s[i * sstride + j];
        }
    }
}

void merge_line(const double * evens, const double * odds, size_t x, size_t width, double * dest) {

    const double * odds_p = odds + (x / 2);
    const double * evens_p = evens + (x / 2);

    size_t dest_x = x;
    if (x % 2 == 1) {
        dest[dest_x++ - x] = *(odds_p++);
    }
    while (dest_x < (x + width) - (x + width) % 2) {
        dest[dest_x++ - x] = *(evens_p++);
        dest[dest_x++ - x] = *(odds_p++);
    }
    if (dest_x < x + width) {
        dest[dest_x++ - x] = *evens_p;
    }
    assert(dest_x == x + width);
}


void get_quadrant_sample(const double * r00, const double * r01, const double * r10, const double * r11,
                         const double * i00, const double * i01, const double * i10, const double * i11,
                         size_t src_stride, size_t dest_stride,
                         size_t x, size_t y, size_t width, size_t height,
                         double * dest_real, double * dest_imag) {
    size_t dest_y = y;
    if (y % 2 == 1) {
        merge_line(&r10[(y / 2) * src_stride], &r11[(y / 2) * src_stride], x, width, dest_real);
        merge_line(&i10[(y / 2) * src_stride], &i11[(y / 2) * src_stride], x, width, dest_imag);
        ++dest_y;
    }
    while (dest_y < (y + height) - (y + height) % 2) {
        merge_line(&r00[(dest_y / 2) * src_stride], &r01[(dest_y / 2) * src_stride], x, width, &dest_real[(dest_y - y) * dest_stride]);
        merge_line(&i00[(dest_y / 2) * src_stride], &i01[(dest_y / 2) * src_stride], x, width, &dest_imag[(dest_y - y) * dest_stride]);
        ++dest_y;
        merge_line(&r10[(dest_y / 2) * src_stride], &r11[(dest_y / 2) * src_stride], x, width, &dest_real[(dest_y - y) * dest_stride]);
        merge_line(&i10[(dest_y / 2) * src_stride], &i11[(dest_y / 2) * src_stride], x, width, &dest_imag[(dest_y - y) * dest_stride]);
        ++dest_y;
    }
    if (dest_y < y + height) {
        merge_line(&r00[(dest_y / 2) * src_stride], &r01[(dest_y / 2) * src_stride], x, width, &dest_real[(dest_y - y) * dest_stride]);
        merge_line(&i00[(dest_y / 2) * src_stride], &i01[(dest_y / 2) * src_stride], x, width, &dest_imag[(dest_y - y) * dest_stride]);
    }
    assert (dest_y == y + height);
}

void merge_line_to_buffer(const double * evens, const double * odds, size_t x, size_t width, double * dest) {

    const double * odds_p = odds + (x / 2);
    const double * evens_p = evens + (x / 2);

    size_t dest_x = x;
    size_t buffer_x = 0;
    if (x % 2 == 1) {
        dest[buffer_x++] = *(odds_p++);
        dest_x++;
    }
    while (dest_x < (x + width) - (x + width) % 2) {
        dest[buffer_x++] = *(evens_p++);
        dest[buffer_x++] = *(odds_p++);
        dest_x++;
        dest_x++;
    }
    if (dest_x < x + width) {
        dest[buffer_x++] = *evens_p;
        dest_x++;
    }
    assert(dest_x == x + width);
}


void get_quadrant_sample_to_buffer(const double * r00, const double * r01, const double * r10, const double * r11,
                                   const double * i00, const double * i01, const double * i10, const double * i11,
                                   size_t src_stride, size_t dest_stride,
                                   size_t x, size_t y, size_t width, size_t height,
                                   double * dest_real, double * dest_imag) {
    size_t dest_y = y;
    size_t buffer_y = 0;
    if (y % 2 == 1) {
        merge_line_to_buffer(&r10[(y / 2) * src_stride], &r11[(y / 2) * src_stride], x, width, dest_real);
        merge_line_to_buffer(&i10[(y / 2) * src_stride], &i11[(y / 2) * src_stride], x, width, dest_imag);
        ++dest_y;
        ++buffer_y;
    }
    while (dest_y < (y + height) - (y + height) % 2) {
        merge_line_to_buffer(&r00[(dest_y / 2) * src_stride], &r01[(dest_y / 2) * src_stride], x, width, &dest_real[buffer_y * dest_stride]);
        merge_line_to_buffer(&i00[(dest_y / 2) * src_stride], &i01[(dest_y / 2) * src_stride], x, width, &dest_imag[buffer_y * dest_stride]);
        ++dest_y;
        ++buffer_y;
        merge_line_to_buffer(&r10[(dest_y / 2) * src_stride], &r11[(dest_y / 2) * src_stride], x, width, &dest_real[buffer_y * dest_stride]);
        merge_line_to_buffer(&i10[(dest_y / 2) * src_stride], &i11[(dest_y / 2) * src_stride], x, width, &dest_imag[buffer_y * dest_stride]);
        ++dest_y;
        ++buffer_y;
    }
    if (dest_y < y + height) {
        merge_line_to_buffer(&r00[(dest_y / 2) * src_stride], &r01[(dest_y / 2) * src_stride], x, width, &dest_real[buffer_y * dest_stride]);
        merge_line_to_buffer(&i00[(dest_y / 2) * src_stride], &i01[(dest_y / 2) * src_stride], x, width, &dest_imag[buffer_y * dest_stride]);
    }
    assert (dest_y == y + height);
}

void stamp(Lattice *grid, State *state, int tag_particle, int iterations, 
           int count_snap, const char * output_folder) {

    char * output_filename;
    output_filename = new char[51];
#ifdef HAVE_MPI
    // Set variables for mpi output
    char *data_as_txt;
    int count;

    MPI_File   file;
    MPI_Status status;

    // each number is represented by charspernum chars
    const int chars_per_complex_num = 30;
    MPI_Datatype complex_num_as_string;
    MPI_Type_contiguous(chars_per_complex_num, MPI_CHAR, &complex_num_as_string);
    MPI_Type_commit(&complex_num_as_string);

    const int charspernum = 14;
    MPI_Datatype num_as_string;
    MPI_Type_contiguous(charspernum, MPI_CHAR, &num_as_string);
    MPI_Type_commit(&num_as_string);

    // create a type describing our piece of the array
    int globalsizes[2] = {grid->global_dim_y - 2 * grid->periods[0] * grid->halo_y, grid->global_dim_x - 2 * grid->periods[1] * grid->halo_x};
    int localsizes [2] = {grid->inner_end_y - grid->inner_start_y, grid->inner_end_x - grid->inner_start_x};
    int starts[2]      = {grid->inner_start_y, grid->inner_start_x};
    int order          = MPI_ORDER_C;

    MPI_Datatype complex_localarray;
    MPI_Type_create_subarray(2, globalsizes, localsizes, starts, order, complex_num_as_string, &complex_localarray);
        MPI_Type_commit(&complex_localarray);
    MPI_Datatype localarray;
    MPI_Type_create_subarray(2, globalsizes, localsizes, starts, order, num_as_string, &localarray);
    MPI_Type_commit(&localarray);

    // output complex matrix
    // conversion
    data_as_txt = new char[(grid->inner_end_x - grid->inner_start_x) * (grid->inner_end_y - grid->inner_start_y) * chars_per_complex_num];
    count = 0;
    for (int j = grid->inner_start_y - grid->start_y; j < grid->inner_end_y - grid->start_y; j++) {
        for (int k = grid->inner_start_x - grid->start_x; k < grid->inner_end_x - grid->start_x - 1; k++) {
            sprintf(&data_as_txt[count * chars_per_complex_num], "(%+.5e,%+.5e)   ", state->p_real[j * grid->dim_x + k], state->p_imag[j * grid->dim_x + k]);
            count++;
        }
        if(grid->mpi_coords[1] == grid->mpi_dims[1] - 1) {
            sprintf(&data_as_txt[count * chars_per_complex_num], "(%+.5e,%+.5e)\n  ", state->p_real[j * grid->dim_x + (grid->inner_end_x - grid->start_x) - 1], state->p_imag[j * grid->dim_x + (grid->inner_end_x - grid->start_x) - 1]);
            count++;
        }
        else {
            sprintf(&data_as_txt[count * chars_per_complex_num], "(%+.5e,%+.5e)   ", state->p_real[j * grid->dim_x + (grid->inner_end_x - grid->start_x) - 1], state->p_imag[j * grid->dim_x + (grid->inner_end_x - grid->start_x) - 1]);
            count++;
        }
    }

    // open the file, and set the view
    sprintf(output_filename, "%s/%i-%i-iter-comp.dat", output_folder, tag_particle + 1, iterations * count_snap);
    MPI_File_open(grid->cartcomm, output_filename,
                  MPI_MODE_CREATE | MPI_MODE_WRONLY,
                  MPI_INFO_NULL, &file);

    MPI_File_set_view(file, 0,  MPI_CHAR, complex_localarray, "native", MPI_INFO_NULL);

    MPI_File_write_all(file, data_as_txt, (grid->inner_end_x - grid->inner_start_x) * (grid->inner_end_y - grid->inner_start_y), complex_num_as_string, &status);
    MPI_File_close(&file);
    delete [] data_as_txt;

    // output real matrix
    //conversion
    data_as_txt = new char[(grid->inner_end_x - grid->inner_start_x) * (grid->inner_end_y - grid->inner_start_y) * charspernum];
    count = 0;
    for (int j = grid->inner_start_y - grid->start_y; j < grid->inner_end_y - grid->start_y; j++) {
        for (int k = grid->inner_start_x - grid->start_x; k < grid->inner_end_x - grid->start_x - 1; k++) {
            sprintf(&data_as_txt[count * charspernum], "%+.5e  ", state->p_real[j * grid->dim_x + k]);
            count++;
        }
        if(grid->mpi_coords[1] == grid->mpi_dims[1] - 1) {
            sprintf(&data_as_txt[count * charspernum], "%+.5e\n ", state->p_real[j * grid->dim_x + (grid->inner_end_x - grid->start_x) - 1]);
            count++;
        }
        else {
            sprintf(&data_as_txt[count * charspernum], "%+.5e  ", state->p_real[j * grid->dim_x + (grid->inner_end_x - grid->start_x) - 1]);
            count++;
        }
    }

    // open the file, and set the view
    sprintf(output_filename, "%s/%i-%i-iter-real.dat", output_folder, tag_particle + 1, iterations * count_snap);
    MPI_File_open(grid->cartcomm, output_filename,
                  MPI_MODE_CREATE | MPI_MODE_WRONLY,
                  MPI_INFO_NULL, &file);

    MPI_File_set_view(file, 0,  MPI_CHAR, localarray, "native", MPI_INFO_NULL);

    MPI_File_write_all(file, data_as_txt, (grid->inner_end_x - grid->inner_start_x) * ( grid->inner_end_y - grid->inner_start_y), num_as_string, &status);
    MPI_File_close(&file);
    delete [] data_as_txt;
#else
    sprintf(output_filename, "%s/%i-%i-iter-real.dat", output_folder, tag_particle + 1, iterations * count_snap);
    print_matrix(output_filename, &(state->p_real[grid->global_dim_x * (inner_start_y - start_y) + inner_start_x - start_x]), grid->global_dim_x,
                 grid->global_dim_x - 2 * grid->periods[1]*grid->halo_x, grid->global_dim_y - 2 * grid->periods[0]*grid->halo_y);

    sprintf(output_filename, "%s/%i-%i-iter-comp.dat", output_folder, tag_particle + 1, iterations * count_snap);
    print_complex_matrix(output_filename, &(state->p_real[grid->global_dim_x * (inner_start_y - start_y) + inner_start_x - start_x]), &(state->p_imag[grid->global_dim_x * (inner_start_y - start_y) + inner_start_x - start_x]), grid->global_dim_x,
                         grid->global_dim_x - 2 * grid->periods[1]*grid->halo_x, grid->global_dim_y - 2 * grid->periods[0]*grid->halo_y);
#endif
    return;
}

void stamp_real(Lattice *grid, double *matrix, int iterations, const char * output_folder, const char * file_tag) {

    char * output_filename;
    output_filename = new char[51];
#ifdef HAVE_MPI
    // Set variables for mpi output
    char *data_as_txt;
    int count;

    MPI_File   file;
    MPI_Status status;

    // each number is represented by charspernum chars
    const int charspernum = 14;
    MPI_Datatype num_as_string;
    MPI_Type_contiguous(charspernum, MPI_CHAR, &num_as_string);
    MPI_Type_commit(&num_as_string);

    // create a type describing our piece of the array
    int globalsizes[2] = {grid->global_dim_y - 2 * grid->periods[0] * grid->halo_y, grid->global_dim_x - 2 * grid->periods[1] * grid->halo_x};
    int localsizes [2] = {grid->inner_end_y - grid->inner_start_y, grid->inner_end_x - grid->inner_start_x};
    int starts[2]      = {grid->inner_start_y, grid->inner_start_x};
    int order          = MPI_ORDER_C;

    MPI_Datatype localarray;
    MPI_Type_create_subarray(2, globalsizes, localsizes, starts, order, num_as_string, &localarray);
    MPI_Type_commit(&localarray);

    // output real matrix
    //conversion
    data_as_txt = new char[(grid->inner_end_x - grid->inner_start_x) * (grid->inner_end_y - grid->inner_start_y) * charspernum];
    count = 0;
    for (int j = grid->inner_start_y - grid->start_y; j < grid->inner_end_y - grid->start_y; j++) {
        for (int k = grid->inner_start_x - grid->start_x; k < grid->inner_end_x - grid->start_x - 1; k++) {
            sprintf(&data_as_txt[count * charspernum], "%+.5e  ", matrix[j * grid->dim_x + k]);
            count++;
        }
        if(grid->mpi_coords[1] == grid->mpi_dims[1] - 1) {
            sprintf(&data_as_txt[count * charspernum], "%+.5e\n ", matrix[j * grid->dim_x + (grid->inner_end_x - grid->start_x) - 1]);
            count++;
        }
        else {
            sprintf(&data_as_txt[count * charspernum], "%+.5e  ", matrix[j * grid->dim_x + (grid->inner_end_x - grid->start_x) - 1]);
            count++;
        }
    }

    // open the file, and set the view
    sprintf(output_filename, "%s/%i-%s", output_folder, iterations, file_tag);
    MPI_File_open(grid->cartcomm, output_filename,
                  MPI_MODE_CREATE | MPI_MODE_WRONLY,
                  MPI_INFO_NULL, &file);

    MPI_File_set_view(file, 0,  MPI_CHAR, localarray, "native", MPI_INFO_NULL);

    MPI_File_write_all(file, data_as_txt, (grid->inner_end_x - grid->inner_start_x) * (grid->inner_end_y - grid->inner_start_y), num_as_string, &status);
    MPI_File_close(&file);
    delete [] data_as_txt;
#else
    sprintf(output_filename, "%s/%i-%s", output_folder, iterations, file_tag);
    print_matrix(output_filename, &(matrix[grid->global_dim_x * (grid->inner_start_y - grid->start_y) + grid->inner_start_x - grid->start_x]), grid->global_dim_x,
                 grid->global_dim_x - 2 * grid->periods[1]*grid->halo_x, grid->global_dim_y - 2 * grid->periods[0]*grid->halo_y);
#endif
    return;
}

void expect_values(int dimx, int dimy, double delta_x, double delta_y, double delta_t, double coupling_const, int iterations, int snapshots, double * hamilt_pot, double particle_mass,
                   const char *dirname, int *periods, int halo_x, int halo_y, energy_momentum_statistics *sample) {

	int dim = dimx; //provvisional
    if(snapshots == 0)
        return;

    int N_files = snapshots + 1;
    int *N_name = new int[N_files];

    N_name[0] = 0;
    for(int i = 1; i < N_files; i++) {
        N_name[i] = N_name[i - 1] + iterations;
    }

    std::complex<double> sum_E = 0;
    std::complex<double> sum_Px = 0, sum_Py = 0;
    std::complex<double> sum_pdi = 0;
    std::complex<double> sum_x2 = 0, sum_x = 0, sum_y2 = 0, sum_y = 0;
    double *energy = new double[N_files];
    double *momentum_x = new double[N_files];
    double *momentum_y = new double[N_files];

    std::complex<double> cost_E = -1. / (2. * particle_mass * delta_x * delta_y), cost_P_x, cost_P_y;
    cost_P_x = std::complex<double>(0., -0.5 / delta_x);
    cost_P_y = std::complex<double>(0., -0.5 / delta_y);

    std::stringstream filename;
    std::string filenames;

    filename.str("");
    filename << dirname << "/exp_val_D" << dim << "_I" << iterations << "_S" << snapshots << ".dat";
    filenames = filename.str();
    std::ofstream out(filenames.c_str());
    
    double E_before = 0, E_now = 0;

    out << "#iter\t time\tEnergy\t\tdelta_E\t\tPx\tPy\tP**2\tnorm2(psi(t))\tsigma_x\tsigma_y\t<X>\t<Y>" << std::endl;
    for(int i = 0; i < N_files; i++) {

        filename.str("");
        filename << dirname << "/" << "1-" << N_name[i] << "-iter-comp.dat";
        filenames = filename.str();
        std::ifstream up(filenames.c_str()), center(filenames.c_str()), down(filenames.c_str());
        std::complex<double> psi_up, psi_down, psi_center, psi_left, psi_right;

        for (int j = 0; j < dim; j++)
            center >> psi_center;
        for (int j = 0; j < 2 * dim; j++)
            down >> psi_center;
        up >> psi_up;
        down >> psi_down;
        center >> psi_left >> psi_center;
        for (int j = 1; j < dim - 1; j++) {
            if(j != 1) {
                up >> psi_up >> psi_up;
                down >> psi_down >> psi_down;
                center >> psi_left >> psi_center;
            }
            for (int k = 1; k < dim - 1; k++) {
                up >> psi_up;
                center >> psi_right;
                down >> psi_down;

                sum_E += conj(psi_center) * (cost_E * (psi_right + psi_left + psi_down + psi_up - psi_center * std::complex<double> (4., 0.)) + psi_center * std::complex<double> (hamilt_pot[j * dim + k], 0.)  + psi_center * psi_center * conj(psi_center) * std::complex<double> (0.5 * coupling_const, 0.)) ;
                sum_Px += conj(psi_center) * (psi_right - psi_left);
                sum_Py += conj(psi_center) * (psi_down - psi_up);
                sum_x2 += conj(psi_center) * std::complex<double> (k * k, 0.) * psi_center;  
                sum_x += conj(psi_center) * std::complex<double> (k, 0.) * psi_center;
                sum_y2 += conj(psi_center) * std::complex<double> (j * j, 0.) * psi_center;
                sum_y += conj(psi_center) * std::complex<double> (j, 0.) * psi_center;
                sum_pdi += conj(psi_center) * psi_center;

                psi_left = psi_center;
                psi_center = psi_right;
            }

        }
        up.close();
        center.close();
        down.close();

        //out << N_name[i] << "\t" << real(sum_E / sum_pdi) << "\t" << real(cost_P_x * sum_Px / sum_pdi) << "\t" << real(cost_P_y * sum_Py / sum_pdi) << "\t"
          //  << real(cost_P * sum_Px / sum_pdi)*real(cost_P * sum_Px / sum_pdi) + real(cost_P * sum_Py / sum_pdi)*real(cost_P * sum_Py / sum_pdi) << "\t" << real(sum_pdi) << std::endl;
		E_now = real(sum_E / sum_pdi);
        out << N_name[i] << "\t" << N_name[i] * delta_t << "\t" << std::setw(10) << real(sum_E / sum_pdi);
        out << "\t" << std::setw(10) << E_before - E_now;
        out << "\t" << std::setw(10) << real(cost_P_x * sum_Px / sum_pdi) << "\t" << std::setw(10) << real(cost_P_y * sum_Py / sum_pdi) << "\t" << std::setw(10)
            << real(cost_P_x * sum_Px / sum_pdi)*real(cost_P_x * sum_Px / sum_pdi) + real(cost_P_y * sum_Py / sum_pdi)*real(cost_P_y * sum_Py / sum_pdi) << "\t" 
            << real(sum_pdi) * delta_x * delta_y << "\t" << delta_x * sqrt(real(sum_x2 / sum_pdi - sum_x * sum_x / (sum_pdi * sum_pdi))) << "\t" << delta_y * sqrt(real(sum_y2 / sum_pdi - sum_y * sum_y / (sum_pdi * sum_pdi)))
            << std::setw(10) << delta_x * real(sum_x / sum_pdi) << "\t" << delta_y * real(sum_y / sum_pdi);
        out << std::endl;
        E_before = E_now;
        
        energy[i] = real(sum_E / sum_pdi);
        momentum_x[i] = real(cost_P_x * sum_Px / sum_pdi);
        momentum_y[i] = real(cost_P_y * sum_Py / sum_pdi);

        sum_E = 0;
        sum_Px = 0;
        sum_Py = 0;
        sum_pdi = 0;
        sum_x2 = 0; sum_x = 0;
        sum_y2 = 0; sum_y = 0;
    }

    //calculate sample mean and sample variance
    for(int i = 0; i < N_files; i++) {
        sample->mean_E += energy[i];
        sample->mean_Px += momentum_x[i];
        sample->mean_Py += momentum_y[i];
    }
    sample->mean_E /= N_files;
    sample->mean_Px /= N_files;
    sample->mean_Py /= N_files;

    for(int i = 0; i < N_files; i++) {
        sample->var_E += (energy[i] - sample->mean_E) * (energy[i] - sample->mean_E);
        sample->var_Px += (momentum_x[i] - sample->mean_Px) * (momentum_x[i] - sample->mean_Px);
        sample->var_Py += (momentum_y[i] - sample->mean_Py) * (momentum_y[i] - sample->mean_Py);
    }
    sample->var_E /= N_files - 1;
    sample->var_E = sqrt(sample->var_E);
    sample->var_Px /= N_files - 1;
    sample->var_Px = sqrt(sample->var_Px);
    sample->var_Py /= N_files - 1;
    sample->var_Py = sqrt(sample->var_Py);
}

double calculate_total_energy(Lattice *grid, State *state, 
                              Hamiltonian *hamiltonian, 
                              double (*hamilt_pot)(int x, int y, Lattice *grid), 
                              double * external_pot, double norm2, bool global) {
	
    int ini_halo_x = grid->inner_start_x - grid->start_x;
    int ini_halo_y = grid->inner_start_y - grid->start_y;
    int end_halo_x = grid->end_x - grid->inner_end_x;
    int end_halo_y = grid->end_y - grid->inner_end_y;
    int tile_width = grid->end_x - grid->start_x;
    
    if(norm2 == 0)
        norm2 = state->calculate_squared_norm(false);
      
    std::complex<double> sum = 0;
    std::complex<double> cost_E = -1. / (2. * hamiltonian->mass);
    std::complex<double> psi_up, psi_down, psi_center, psi_left, psi_right;
    std::complex<double> rot_y, rot_x;
    double cost_rot_x = 0.5 * hamiltonian->omega * grid->delta_y / grid->delta_x;
    double cost_rot_y = 0.5 * hamiltonian->omega * grid->delta_x / grid->delta_y;

    for (int i = grid->inner_start_y - grid->start_y + (ini_halo_y == 0), 
         y = grid->inner_start_y + (ini_halo_y == 0); 
         i < grid->inner_end_y - grid->start_y - (end_halo_y == 0); i++, y++) {
        for (int j = grid->inner_start_x - grid->start_x + (ini_halo_x == 0), 
             x = grid->inner_start_x + (ini_halo_x == 0); 
             j < grid->inner_end_x - grid->start_x - (end_halo_x == 0); j++, x++) {
            std::complex<double> potential_term;
            if(external_pot == NULL) {
                potential_term = std::complex<double> (hamilt_pot(x, y, grid), 0.);
            } else {
                potential_term = std::complex<double> (external_pot[y * grid->global_dim_x + x], 0.);
            }
            psi_center = std::complex<double> (state->p_real[i * tile_width + j], 
                                               state->p_imag[i * tile_width + j]);
            psi_up = std::complex<double> (state->p_real[(i - 1) * tile_width + j], 
                                           state->p_imag[(i - 1) * tile_width + j]);
            psi_down = std::complex<double> (state->p_real[(i + 1) * tile_width + j], 
                                             state->p_imag[(i + 1) * tile_width + j]);
            psi_right = std::complex<double> (state->p_real[i * tile_width + j + 1], 
                                              state->p_imag[i * tile_width + j + 1]);
            psi_left = std::complex<double> (state->p_real[i * tile_width + j - 1], 
                                             state->p_imag[i * tile_width + j - 1]);
            
            rot_x = std::complex<double>(0., cost_rot_x * (y - hamiltonian->rot_coord_y));
            rot_y = std::complex<double>(0., cost_rot_y * (x - hamiltonian->rot_coord_x));
            sum += conj(psi_center) * (cost_E * (
                std::complex<double> (1. / (grid->delta_x * grid->delta_x), 0.) * 
                  (psi_right + psi_left - psi_center * std::complex<double> (2., 0.)) + 
                std::complex<double> (1. / (grid->delta_y * grid->delta_y), 0.) * 
                  (psi_down + psi_up - psi_center * std::complex<double> (2., 0.))) + 
                psi_center * potential_term + 
                psi_center * psi_center * conj(psi_center) * std::complex<double> (0.5 * hamiltonian->coupling_a, 0.) + 
                rot_y * (psi_down - psi_up) - rot_x * (psi_right - psi_left));
        }
    }
	double total_energy = real(sum / norm2) * grid->delta_x * grid->delta_y;
#ifdef HAVE_MPI
    if (global) {
        double *sums = new double[grid->mpi_procs];
        MPI_Allgather(&total_energy, 1, MPI_DOUBLE, sums, 1, MPI_DOUBLE, grid->cartcomm);
        sums[0] = total_energy;
        total_energy = 0.;
        for(int i = 0; i < grid->mpi_procs; i++)
            total_energy += sums[i];
        delete [] sums;
    } 
#endif   
  return total_energy;
}

double calculate_kinetic_energy(Lattice *grid, State *state, Hamiltonian *hamiltonian, double norm2, bool global) {
	
	int ini_halo_x = grid->inner_start_x - grid->start_x;
	int ini_halo_y = grid->inner_start_y - grid->start_y;
	int end_halo_x = grid->end_x - grid->inner_end_x;
	int end_halo_y = grid->end_y - grid->inner_end_y;
	int tile_width = grid->end_x - grid->start_x;
	
	if(norm2 == 0)
      norm2 = state->calculate_squared_norm(false);
      
	std::complex<double> sum = 0;
	std::complex<double> cost_E = -1. / (2. * hamiltonian->mass);
	std::complex<double> psi_up, psi_down, psi_center, psi_left, psi_right;
	for (int i = grid->inner_start_y - grid->start_y + (ini_halo_y == 0); 
       i < grid->inner_end_y - grid->start_y - (end_halo_y == 0); i++) {
      for (int j = grid->inner_start_x - grid->start_x + (ini_halo_x == 0); 
           j < grid->inner_end_x - grid->start_x - (end_halo_x == 0); j++) {
          psi_center = std::complex<double> (state->p_real[i * tile_width + j], 
                                             state->p_imag[i * tile_width + j]);
          psi_up = std::complex<double> (state->p_real[(i - 1) * tile_width + j], 
                                         state->p_imag[(i - 1) * tile_width + j]);
          psi_down = std::complex<double> (state->p_real[(i + 1) * tile_width + j], 
                                           state->p_imag[(i + 1) * tile_width + j]);
          psi_right = std::complex<double> (state->p_real[i * tile_width + j + 1], 
                                            state->p_imag[i * tile_width + j + 1]);
          psi_left = std::complex<double> (state->p_real[i * tile_width + j - 1], 
                                           state->p_imag[i * tile_width + j - 1]);
          sum += conj(psi_center) * (cost_E * 
            (std::complex<double>(1./(grid->delta_x * grid->delta_x), 0.) * 
               (psi_right + psi_left - psi_center * std::complex<double>(2., 0.)) + 
             std::complex<double>(1./(grid->delta_y * grid->delta_y), 0.) * 
               (psi_down + psi_up - psi_center * std::complex<double> (2., 0.))));
		}
	}
	double kinetic_energy =  real(sum / norm2) * grid->delta_x * grid->delta_y;
#ifdef HAVE_MPI
    if (global) {
        double *sums = new double[grid->mpi_procs];
        MPI_Allgather(&kinetic_energy, 1, MPI_DOUBLE, sums, 1, MPI_DOUBLE, grid->cartcomm);
        sums[0] = kinetic_energy;
        kinetic_energy = 0.;
        for(int i = 0; i < grid->mpi_procs; i++)
            kinetic_energy += sums[i];
        delete [] sums;
    } 
#endif 
  return kinetic_energy;
}

double calculate_rotational_energy(Lattice *grid, State *state, Hamiltonian *hamiltonian, double norm2, bool global) {
					  
	int ini_halo_x = grid->inner_start_x - grid->start_x;
	int ini_halo_y = grid->inner_start_y - grid->start_y;
	int end_halo_x = grid->end_x - grid->inner_end_x;
	int end_halo_y = grid->end_y - grid->inner_end_y;
	int tile_width = grid->end_x - grid->start_x;
	
	if(norm2 == 0)
      norm2 = state->calculate_squared_norm(false);
	std::complex<double> sum = 0;
	std::complex<double> psi_up, psi_down, psi_center, psi_left, psi_right;
	std::complex<double> rot_y, rot_x;
	double cost_rot_x = 0.5 * hamiltonian->omega * grid->delta_y / grid->delta_x;
	double cost_rot_y = 0.5 * hamiltonian->omega * grid->delta_x / grid->delta_y;
	for(int i = grid->inner_start_y - grid->start_y + (ini_halo_y == 0), 
          y = grid->inner_start_y + (ini_halo_y == 0); 
          i <grid->inner_end_y - grid->start_y - (end_halo_y == 0); i++, y++) {
      for(int j = grid->inner_start_x - grid->start_x + (ini_halo_x == 0), 
              x = grid->inner_start_x + (ini_halo_x == 0); 
              j < grid->inner_end_x - grid->start_x - (end_halo_x == 0); j++, x++) {
          psi_center = std::complex<double> (state->p_real[i * tile_width + j], 
                                             state->p_imag[i * tile_width + j]);
          psi_up = std::complex<double> (state->p_real[(i - 1) * tile_width + j], 
                                         state->p_imag[(i - 1) * tile_width + j]);
          psi_down = std::complex<double> (state->p_real[(i + 1) * tile_width + j], 
                                           state->p_imag[(i + 1) * tile_width + j]);
          psi_right = std::complex<double> (state->p_real[i * tile_width + j + 1], 
                                            state->p_imag[i * tile_width + j + 1]);
          psi_left = std::complex<double> (state->p_real[i * tile_width + j - 1], 
                                           state->p_imag[i * tile_width + j - 1]);
          
          rot_x = std::complex<double>(0., cost_rot_x * (y - hamiltonian->rot_coord_y));
          rot_y = std::complex<double>(0., cost_rot_y * (x - hamiltonian->rot_coord_x));
          sum += conj(psi_center) * (rot_y * (psi_down - psi_up) - rot_x * (psi_right - psi_left)) ;
		}
	}
	
	double energy_rot = real(sum / norm2) * grid->delta_x * grid->delta_y;
#ifdef HAVE_MPI
    if (global) {
        double *sums = new double[grid->mpi_procs];
        MPI_Allgather(&energy_rot, 1, MPI_DOUBLE, sums, 1, MPI_DOUBLE, grid->cartcomm);
        sums[0] = energy_rot;
        energy_rot = 0.;
        for(int i = 0; i < grid->mpi_procs; i++)
            energy_rot += sums[i];
        delete [] sums;
    } 
#endif 
    return energy_rot;
}

void calculate_mean_position(Lattice *grid, State *state, int grid_origin_x, int grid_origin_y, 
                             double *results, double norm2) {
	
    int ini_halo_x = grid->inner_start_x - grid->start_x;
    int ini_halo_y = grid->inner_start_y - grid->start_y;
    int end_halo_x = grid->end_x - grid->inner_end_x;
    int end_halo_y = grid->end_y - grid->inner_end_y;
    int tile_width = grid->end_x - grid->start_x;
    
    if(norm2 == 0)
        norm2 = state->calculate_squared_norm(false);
            
    std::complex<double> sum_x_mean = 0, sum_xx_mean = 0, sum_y_mean = 0, sum_yy_mean = 0;
    std::complex<double> psi_center;
    for (int i = grid->inner_start_y - grid->start_y + (ini_halo_y == 0); 
         i < grid->inner_end_y - grid->start_y - (end_halo_y == 0); i++) {
        for (int j = grid->inner_start_x - grid->start_x + (ini_halo_x == 0); 
             j < grid->inner_end_x - grid->start_x - (end_halo_x == 0); j++) {
            psi_center = std::complex<double> (state->p_real[i * tile_width + j], state->p_imag[i * tile_width + j]);
            sum_x_mean += conj(psi_center) * psi_center * std::complex<double>(grid->delta_x * (j - grid_origin_x), 0.);
            sum_y_mean += conj(psi_center) * psi_center * std::complex<double>(grid->delta_y * (i - grid_origin_y), 0.);
            sum_xx_mean += conj(psi_center) * psi_center * std::complex<double>(grid->delta_x * (j - grid_origin_x), 0.) * std::complex<double>(grid->delta_x * (j - grid_origin_x), 0.);
            sum_yy_mean += conj(psi_center) * psi_center * std::complex<double>(grid->delta_y * (i - grid_origin_y), 0.) * std::complex<double>(grid->delta_y * (i - grid_origin_y), 0.);
      }
    }
    
    results[0] = real(sum_x_mean / norm2) * grid->delta_x * grid->delta_y;
    results[2] = real(sum_y_mean / norm2) * grid->delta_x * grid->delta_y;
    results[1] = real(sum_xx_mean / norm2) * grid->delta_x * grid->delta_y - results[0] * results[0];
    results[3] = real(sum_yy_mean / norm2) * grid->delta_x * grid->delta_y - results[2] * results[2];
}

void calculate_mean_momentum(Lattice *grid, State *state, double *results,
                             double norm2) {
	
    int ini_halo_x = grid->inner_start_x - grid->start_x;
    int ini_halo_y = grid->inner_start_y - grid->start_y;
    int end_halo_x = grid->end_x - grid->inner_end_x;
    int end_halo_y = grid->end_y - grid->inner_end_y;
    int tile_width = grid->end_x - grid->start_x;
    
    if(norm2 == 0)
        norm2 = state->calculate_squared_norm(false);
      
    std::complex<double> sum_px_mean = 0, sum_pxpx_mean = 0, sum_py_mean = 0, 
                         sum_pypy_mean = 0, 
                         var_px = std::complex<double>(0., - 0.5 / grid->delta_x), 
                         var_py = std::complex<double>(0., - 0.5 / grid->delta_y);
    std::complex<double> psi_up, psi_down, psi_center, psi_left, psi_right;
    for (int i = grid->inner_start_y - grid->start_y + (ini_halo_y == 0); 
         i < grid->inner_end_y - grid->start_y - (end_halo_y == 0); i++) {
        for (int j = grid->inner_start_x - grid->start_x + (ini_halo_x == 0); 
             j < grid->inner_end_x - grid->start_x - (end_halo_x == 0); j++) {
            psi_center = std::complex<double> (state->p_real[i * tile_width + j], 
                                               state->p_imag[i * tile_width + j]);
            psi_up = std::complex<double> (state->p_real[(i - 1) * tile_width + j], 
                                           state->p_imag[(i - 1) * tile_width + j]);
            psi_down = std::complex<double> (state->p_real[(i + 1) * tile_width + j], 
                                             state->p_imag[(i + 1) * tile_width + j]);
            psi_right = std::complex<double> (state->p_real[i * tile_width + j + 1], 
                                              state->p_imag[i * tile_width + j + 1]);
            psi_left = std::complex<double> (state->p_real[i * tile_width + j - 1], 
                                             state->p_imag[i * tile_width + j - 1]);
            
            sum_px_mean += conj(psi_center) * (psi_right - psi_left);
            sum_py_mean += conj(psi_center) * (psi_up - psi_down);
            sum_pxpx_mean += conj(psi_center) * (psi_right - 2. * psi_center + psi_left);
            sum_pypy_mean += conj(psi_center) * (psi_up - 2. * psi_center + psi_down);
        }
    }

    sum_px_mean = sum_px_mean * var_px;
    sum_py_mean = sum_py_mean * var_py;
    sum_pxpx_mean = sum_pxpx_mean * (-1.)/(grid->delta_x * grid->delta_x);
    sum_pypy_mean = sum_pypy_mean * (-1.)/(grid->delta_y * grid->delta_y);
    
    results[0] = real(sum_px_mean / norm2) * grid->delta_x * grid->delta_y;
    results[2] = real(sum_py_mean / norm2) * grid->delta_x * grid->delta_y;
    results[1] = real(sum_pxpx_mean / norm2) * grid->delta_x * grid->delta_y - results[0] * results[0];
    results[3] = real(sum_pypy_mean / norm2) * grid->delta_x * grid->delta_y - results[2] * results[2];
}

double calculate_rabi_coupling_energy(Lattice *grid, State *state1, State *state2, double omega_r, double omega_i, double norm2) {
	int ini_halo_x = grid->inner_start_x - grid->start_x;
	int ini_halo_y = grid->inner_start_y - grid->start_y;
	int end_halo_x = grid->end_x - grid->inner_end_x;
	int end_halo_y = grid->end_y - grid->inner_end_y;
	int tile_width = grid->end_x - grid->start_x;
	
	if(norm2 == 0)
      norm2 = state1->calculate_squared_norm(false) + 
              state2->calculate_squared_norm(false);
		
	std::complex<double> sum = 0;
	std::complex<double> psi_center_a, psi_center_b;
	std::complex<double> omega = std::complex<double> (omega_r, omega_i);
	
	for (int i = grid->inner_start_y - grid->start_y + (ini_halo_y == 0), 
       y = grid->inner_start_y + (ini_halo_y == 0); 
       i < grid->inner_end_y - grid->start_y - (end_halo_y == 0); i++, y++) {
      for (int j = grid->inner_start_x - grid->start_x + (ini_halo_x == 0), 
           x = grid->inner_start_x + (ini_halo_x == 0); 
           j < grid->inner_end_x - grid->start_x - (end_halo_x == 0); j++, x++) {
          psi_center_a = std::complex<double> (state1->p_real[i * tile_width + j], 
                                               state1->p_imag[i * tile_width + j]);
          psi_center_b = std::complex<double> (state2->p_real[i * tile_width + j], 
                                               state2->p_imag[i * tile_width + j]);
          sum += conj(psi_center_a) * psi_center_b * omega +  conj(psi_center_b) * psi_center_a * conj(omega);
		}
	}
	
	return real(sum / norm2) * grid->delta_x * grid->delta_y;
}

double calculate_ab_energy(Lattice *grid, State *state1, State *state2, 
                 double coupling_const_ab, double norm2) {
	int ini_halo_x = grid->inner_start_x - grid->start_x;
	int ini_halo_y = grid->inner_start_y - grid->start_y;
	int end_halo_x = grid->end_x - grid->inner_end_x;
	int end_halo_y = grid->end_y - grid->inner_end_y;
	int tile_width = grid->end_x - grid->start_x;
	
	if(norm2 == 0)
      norm2 = state1->calculate_squared_norm(false) + 
              state2->calculate_squared_norm(false);
		
	std::complex<double> sum = 0;
	std::complex<double> psi_center_a, psi_center_b;
	
	for (int i = grid->inner_start_y - grid->start_y + (ini_halo_y == 0), 
       y = grid->inner_start_y + (ini_halo_y == 0); 
       i < grid->inner_end_y - grid->start_y - (end_halo_y == 0); i++, y++) {
		  for (int j = grid->inner_start_x - grid->start_x + (ini_halo_x == 0), 
           x = grid->inner_start_x + (ini_halo_x == 0); 
           j < grid->inner_end_x - grid->start_x - (end_halo_x == 0); j++, x++) {
          psi_center_a = std::complex<double> (state1->p_real[i * tile_width + j], state1->p_imag[i * tile_width + j]);
          psi_center_b = std::complex<double> (state2->p_real[i * tile_width + j], state2->p_imag[i * tile_width + j]);
          sum += conj(psi_center_a) * psi_center_a * conj(psi_center_b) * psi_center_b * std::complex<double> (coupling_const_ab);
		}
	}
	return real(sum / norm2) * grid->delta_x * grid->delta_y;
}

double calculate_total_energy(Lattice *grid, State *state1, State *state2, 
                              Hamiltonian2Component *hamiltonian,
                              double (*hamilt_pot_a)(int x, int y, Lattice *grid),
                              double (*hamilt_pot_b)(int x, int y, Lattice *grid), 
                              double **external_pot, double norm2, bool global) {
					
	if(external_pot == NULL) {
		external_pot = new double* [2];
		external_pot[0] = NULL;
		external_pot[1] = NULL;
	}
	double sum = 0;
	if(norm2 == 0)
		norm2 = state1->calculate_squared_norm() + state2->calculate_squared_norm();
	
  Hamiltonian *hamiltonian_b = new Hamiltonian(grid, hamiltonian->mass_b, hamiltonian->coupling_b, 
                hamiltonian->coupling_ab, hamiltonian->angular_velocity, 
                hamiltonian->rot_coord_x, hamiltonian->rot_coord_y, 
                hamiltonian->omega, hamiltonian->external_pot);
	sum += calculate_total_energy(grid, state1, hamiltonian, hamilt_pot_a, external_pot[0], norm2);
	sum += calculate_total_energy(grid, state2, hamiltonian_b, hamilt_pot_b, external_pot[1], norm2);
	sum += calculate_ab_energy(grid, state1, state2, hamiltonian->coupling_ab, norm2);
	sum += calculate_rabi_coupling_energy(grid, state1, state2, hamiltonian->omega_r, hamiltonian->omega_i, norm2);
  delete hamiltonian_b;
	return sum;
}

Lattice::Lattice(int dim, double _delta_x, double _delta_y, int _periods[2], 
                 double omega): delta_x(_delta_x), delta_y(_delta_y) {
    if (_periods == 0) {
          periods[0] = 0;
          periods[1] = 0;
    } else {
          periods[0] = _periods[0];
          periods[1] = _periods[1];
    }
#ifdef HAVE_MPI
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_procs);
    MPI_Dims_create(mpi_procs, 2, mpi_dims);  //partition all the processes (the size of MPI_COMM_WORLD's group) into an 2-dimensional topology
    MPI_Cart_create(MPI_COMM_WORLD, 2, mpi_dims, periods, 0, &cartcomm);
    MPI_Comm_rank(cartcomm, &mpi_rank);
    MPI_Cart_coords(cartcomm, mpi_rank, 2, mpi_coords);
#else
    mpi_procs = 1;
    mpi_rank = 0;
    mpi_dims[0] = mpi_dims[1] = 1;
    mpi_coords[0] = mpi_coords[1] = 0;
#endif
    halo_x = (omega == 0. ? 4 : 8);
    halo_y = (omega == 0. ? 4 : 8);
    global_dim_x = dim + periods[1] * 2 * halo_x;
    global_dim_y = dim + periods[0] * 2 * halo_y;
    //set dimension of tiles and offsets
    calculate_borders(mpi_coords[1], mpi_dims[1], &start_x, &end_x, 
                      &inner_start_x, &inner_end_x, 
                      global_dim_x - 2 * periods[1]*halo_x, halo_x, periods[1]);
    calculate_borders(mpi_coords[0], mpi_dims[0], &start_y, &end_y, 
                      &inner_start_y, &inner_end_y, 
                      global_dim_y - 2 * periods[0]*halo_y, halo_y, periods[0]);
    dim_x = end_x - start_x;
    dim_y = end_y - start_y;
}

State::State(Lattice *_grid, double *_p_real, double *_p_imag): grid(_grid){
        if (_p_real == 0) {
            self_init = true;
            p_real = new double[grid->dim_x * grid->dim_y];
        } else {
            self_init = false;
            p_real = _p_real;
        }
        if (_p_imag == 0) {
            p_imag = new double[grid->dim_x * grid->dim_y];
        } else {
            p_imag = _p_imag;
        }
    }
    
State::~State() {
        if (self_init) {
            delete p_real;
            delete p_imag;
        }
    }

void State::init_state(std::complex<double> (*ini_state)(int x, int y, Lattice *grid)) {
    std::complex<double> tmp;
    for (int y = 0, idy = grid->start_y; y < grid->dim_y; y++, idy++) {
        for (int x = 0, idx = grid->start_x; x < grid->dim_x; x++, idx++) {
            tmp = ini_state(idx, idy, grid);
            p_real[y * grid->dim_x + x] = real(tmp);
            p_imag[y * grid->dim_x + x] = imag(tmp);
        }
    }
}


void State::read_state(char *file_name, int read_offset) {
    std::ifstream input(file_name);
    int in_width = grid->global_dim_x - 2 * grid->periods[1] * grid->halo_x;
    int in_height = grid->global_dim_y - 2 * grid->periods[0] * grid->halo_y;
    std::complex<double> tmp;
    for(int i = 0; i < read_offset; i++)
        input >> tmp;
    for(int i = 0; i < in_height; i++) {
        for(int j = 0; j < in_width; j++) {
            input >> tmp;
            if((i - grid->start_y) >= 0 && (i - grid->start_y) < grid->dim_y && (j - grid->start_x) >= 0 && (j - grid->start_x) < grid->dim_x) {
                p_real[(i - grid->start_y) * grid->dim_x + j - grid->start_x] = real(tmp);
                p_imag[(i - grid->start_y) * grid->dim_x + j - grid->start_x] = imag(tmp);
            }

            //Down band
            if(i < grid->halo_y && grid->mpi_coords[0] == grid->mpi_dims[0] - 1 && grid->periods[0] != 0) {
                if((j - grid->start_x) >= 0 && (j - grid->start_x) < grid->dim_x) {
                    p_real[(i + grid->dim_y - grid->halo_y) * grid->dim_x + j - grid->start_x] = real(tmp);
                    p_imag[(i + grid->dim_y - grid->halo_y) * grid->dim_x + j - grid->start_x] = imag(tmp);
                }
                //Down right corner
                if(j < grid->halo_x && grid->periods[1] != 0 && grid->mpi_coords[1] == grid->mpi_dims[1] - 1) {
                    p_real[(i + grid->dim_y - grid->halo_y) * grid->dim_x + j + grid->dim_x - grid->halo_x] = real(tmp);
                    p_imag[(i + grid->dim_y - grid->halo_y) * grid->dim_x + j + grid->dim_x - grid->halo_x] = imag(tmp);
                }
                //Down left corner
                if(j >= in_width - grid->halo_x && grid->periods[1] != 0 && grid->mpi_coords[1] == 0) {
                    p_real[(i + grid->dim_y - grid->halo_y) * grid->dim_x + j - (in_width - grid->halo_x)] = real(tmp);
                    p_imag[(i + grid->dim_y - grid->halo_y) * grid->dim_x + j - (in_width - grid->halo_x)] = imag(tmp);
                }
            }

            //Upper band
            if(i >= in_height - grid->halo_y && grid->periods[0] != 0 && grid->mpi_coords[0] == 0) {
                if((j - grid->start_x) >= 0 && (j - grid->start_x) < grid->dim_x) {
                    p_real[(i - (in_height - grid->halo_y)) * grid->dim_x + j - grid->start_x] = real(tmp);
                    p_imag[(i - (in_height - grid->halo_y)) * grid->dim_x + j - grid->start_x] = imag(tmp);
                }
                //Up right corner
                if(j < grid->halo_x && grid->periods[1] != 0 && grid->mpi_coords[1] == grid->mpi_dims[1] - 1) {
                    p_real[(i - (in_height - grid->halo_y)) * grid->dim_x + j + grid->dim_x - grid->halo_x] = real(tmp);
                    p_imag[(i - (in_height - grid->halo_y)) * grid->dim_x + j + grid->dim_x - grid->halo_x] = imag(tmp);
                }
                //Up left corner
                if(j >= in_width - grid->halo_x && grid->periods[1] != 0 && grid->mpi_coords[1] == 0) {
                    p_real[(i - (in_height - grid->halo_y)) * grid->dim_x + j - (in_width - grid->halo_x)] = real(tmp);
                    p_imag[(i - (in_height - grid->halo_y)) * grid->dim_x + j - (in_width - grid->halo_x)] = imag(tmp);
                }
            }

            //Right band
            if(j < grid->halo_x && grid->periods[1] != 0 && grid->mpi_coords[1] == grid->mpi_dims[1] - 1) {
                if((i - grid->start_y) >= 0 && (i - grid->start_y) < grid->dim_y) {
                    p_real[(i - grid->start_y) * grid->dim_x + j + grid->dim_x - grid->halo_x] = real(tmp);
                    p_imag[(i - grid->start_y) * grid->dim_x + j + grid->dim_x - grid->halo_x] = imag(tmp);
                }
            }

            //Left band
            if(j >= in_width - grid->halo_x && grid->periods[1] != 0 && grid->mpi_coords[1] == 0) {
                if((i - grid->start_y) >= 0 && (i - grid->start_y) < grid->dim_y) {
                    p_real[(i - grid->start_y) * grid->dim_x + j - (in_width - grid->halo_x)] = real(tmp);
                    p_imag[(i - grid->start_y) * grid->dim_x + j - (in_width - grid->halo_x)] = imag(tmp);
                }
            }
        }
    }
    input.close();
}

double State::calculate_squared_norm(bool global) {
    double norm2 = 0;
    int tile_width = grid->end_x - grid->start_x;
    for(int i = grid->inner_start_y - grid->start_y; i < grid->inner_end_y - grid->start_y; i++) {
      for(int j = grid->inner_start_x - grid->start_x; j < grid->inner_end_x - grid->start_x; j++) {
        norm2 += p_real[j + i * tile_width] * p_real[j + i * tile_width] + p_imag[j + i * tile_width] * p_imag[j + i * tile_width];
      }
    }
#ifdef HAVE_MPI
    if (global) {
        double *sums = new double[grid->mpi_procs];
        MPI_Allgather(&norm2, 1, MPI_DOUBLE, sums, 1, MPI_DOUBLE, grid->cartcomm);
        sums[0] = norm2;
        norm2 = 0.;
        for(int i = 0; i < grid->mpi_procs; i++)
            norm2 += sums[i];
        delete [] sums;
    } 
#endif 
    return norm2;
    }

double *State::get_particle_density(double *_density) {
        double *density;
        if (_density == 0) { 
          density = new double[grid->dim_x * grid->dim_y];
        } else {
          density = _density;
        }
        for(int j = grid->inner_start_y - grid->start_y; j < grid->inner_end_y - grid->start_y; j++) {
            for(int i = grid->inner_start_x - grid->start_x; i < grid->inner_end_x - grid->start_x; i++) {
                density[j * grid->dim_x + i] = p_real[j * grid->dim_x + i] * p_real[j * grid->dim_x + i] + p_imag[j * grid->dim_x + i] * p_imag[j * grid->dim_x + i];
          }
        }
        return density;
    }

double *State::get_phase(double *_phase) {
        double *phase;
        if (_phase == 0) { 
          phase = new double[grid->dim_x * grid->dim_y];
        } else {
          phase = _phase;
        }
        double norm;
        for(int j = grid->inner_start_y - grid->start_y; j < grid->inner_end_y - grid->start_y; j++) {
            for(int i = grid->inner_start_x - grid->start_x; i < grid->inner_end_x - grid->start_x; i++) {
                norm = sqrt(p_real[j * grid->dim_x + i] * p_real[j * grid->dim_x + i] + p_imag[j * grid->dim_x + i] * p_imag[j * grid->dim_x + i]);
                if(norm == 0)
                    phase[j * grid->dim_x + i] = 0;
                else
                    phase[j * grid->dim_x + i] = acos(p_real[j * grid->dim_x + i] / norm) * ((p_imag[j * grid->dim_x + i] > 0) - (p_imag[j * grid->dim_x + i] < 0));
            }
        }
        return phase;
    }


Hamiltonian::Hamiltonian(Lattice *_grid, double _mass, double _coupling_a, 
                         double _coupling_ab, double _angular_velocity, 
                         double _rot_coord_x, double _rot_coord_y, 
                         double _omega,
                         double *_external_pot): grid(_grid), mass(_mass),
                         coupling_a(_coupling_a), coupling_ab(_coupling_ab),
                         angular_velocity(_angular_velocity), omega(_omega) {
        if (_rot_coord_x == DBL_MAX) {
            rot_coord_x = grid->dim_x * 0.5;
        } else {
            rot_coord_y = _rot_coord_y;
        }
        if (_rot_coord_y == DBL_MAX) {
            rot_coord_y = grid->dim_y * 0.5;
        } else {
            rot_coord_y = _rot_coord_y;
        }
        if (_external_pot == 0) {
            external_pot = new double[grid->dim_y * grid->dim_x];
            self_init = true;
        } else {
            external_pot = _external_pot;
            self_init = false;
        }
    }

Hamiltonian::~Hamiltonian() {
        if (self_init) {
            delete [] external_pot;
        }
    }

void Hamiltonian::initialize_potential(double (*hamiltonian_pot)(int x, int y, Lattice *grid)) {
        for(int y = 0; y < grid->dim_y; y++) {
            for(int x = 0; x < grid->dim_x; x++) {
                external_pot[y * grid->dim_y + x] = hamiltonian_pot(x, y, grid);
            }
        }
    }

Hamiltonian2Component::Hamiltonian2Component(Lattice *_grid, double _mass, 
                         double _mass_b, double _coupling_a, 
                         double _coupling_ab, double _coupling_b,
                         double _angular_velocity, 
                         double _rot_coord_x, double _rot_coord_y, double _omega,
                         double _omega_r, double _omega_i,
                         double *_external_pot, double *_external_pot_b):
                         Hamiltonian(_grid, _mass, _coupling_a, _coupling_ab, _angular_velocity, _rot_coord_x, rot_coord_y, _omega, _external_pot),
                         coupling_b(_coupling_b), omega_r(_omega_r), omega_i(_omega_i) {
        if (_external_pot_b == 0) {
            external_pot_b = new double[grid->dim_y * grid->dim_x];
            self_init = true;
        } else {
            external_pot_b = _external_pot_b;
            self_init = false;
        }
    }

Hamiltonian2Component::~Hamiltonian2Component() {
        if (self_init) {
            delete [] external_pot;          
            delete [] external_pot_b;
        }
    }

void Hamiltonian2Component::initialize_potential_b(double (*hamiltonian_pot)(int x, int y, Lattice *grid)) {
        for(int y = 0; y < grid->dim_y; y++) {
            for(int x = 0; x < grid->dim_x; x++) {
                external_pot_b[y * grid->dim_y + x] = hamiltonian_pot(x, y, grid);
            }
        }
    }
