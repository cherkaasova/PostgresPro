#include "postgres.h"
#include "fmgr.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/numeric.h"
#include "catalog/pg_type.h"
#include "utils/lsyscache.h"

#include <math.h>
#include <stdlib.h>

PG_MODULE_MAGIC;


//Преобразуем числовой массив в обычный массив double
static double *
extract_numeric_array(ArrayType *array, int *nelems)
{
    Datum *elems;
    bool *nulls;
    Oid elem_type;
    int16 typlen;
    bool typbyval;
    char typalign;
    double *nums;
    int i;

    elem_type = ARR_ELEMTYPE(array);

    if (elem_type != INT2OID && elem_type != INT4OID &&
        elem_type != INT8OID && elem_type != FLOAT4OID &&
        elem_type != FLOAT8OID && elem_type != NUMERICOID)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("Массив должен содержать числовые значения")));
    }

    get_typlenbyvalalign(elem_type, &typlen, &typbyval, &typalign);

    deconstruct_array(array, elem_type, typlen, typbyval, typalign,
                      &elems, &nulls, nelems);

    if (*nelems == 0)
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("Массив не может быть пустым")));

    nums = (double *) palloc(sizeof(double) * (*nelems));

    for (i = 0; i < *nelems; i++)
    {
        if (nulls[i])
            ereport(ERROR,
                    (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
                     errmsg("Массив не должен содержать NULL элементов")));

        switch (elem_type)
        {
            case INT2OID:
                nums[i] = (double) DatumGetInt16(elems[i]);
                break;

            case INT4OID:
                nums[i] = (double) DatumGetInt32(elems[i]);
                break;

            case INT8OID:
                nums[i] = (double) DatumGetInt64(elems[i]);
                break;

            case FLOAT4OID:
                nums[i] = (double) DatumGetFloat4(elems[i]);
                break;

            case FLOAT8OID:
                nums[i] = DatumGetFloat8(elems[i]);
                break;

            case NUMERICOID:
                nums[i] = DatumGetFloat8(
                    DirectFunctionCall1(numeric_float8, elems[i]));
                break;
        }

        if (!isfinite(nums[i]))
            ereport(ERROR,
                    (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                     errmsg("Значения массива должны быть конечными числами")));
    }

    pfree(elems);
    pfree(nulls);

    return nums;
}


//среднее массива
static double
calc_mean(double *nums, int nelems)
{
    double sum = 0;
    int i;

    for (i = 0; i < nelems; i++)
        sum += nums[i];

    return sum / nelems;
}


//сумма квадратов отклонений от среднего
static double
calc_sum_sq_diff(double *nums, int nelems, double mean)
{
    double sum = 0;
    double diff;
    int i;

    for (i = 0; i < nelems; i++)
    {
        diff = nums[i] - mean;
        sum += diff * diff;
    }

    return sum;
}


//сравнение двух чисел для qsort
static int
compare_double(const void *a, const void *b)
{
    double x = *(const double *) a;
    double y = *(const double *) b;

    if (x < y)
        return -1;
    if (x > y)
        return 1;

    return 0;
}

//преобразует результат double в numeric
static Datum
double_to_numeric(double value)
{
    return DirectFunctionCall1(float8_numeric, Float8GetDatum(value));
}

//среднее арифметическое
PG_FUNCTION_INFO_V1(array_mean);

Datum
array_mean(PG_FUNCTION_ARGS)
{
    ArrayType *array = PG_GETARG_ARRAYTYPE_P(0);
    int nelems;
    double *nums = extract_numeric_array(array, &nelems);
    double result = calc_mean(nums, nelems);

    if (!isfinite(result)) PG_RETURN_NULL();
    PG_RETURN_DATUM(double_to_numeric(result));
}


//медиана
PG_FUNCTION_INFO_V1(array_median);

Datum
array_median(PG_FUNCTION_ARGS)
{
    ArrayType *array = PG_GETARG_ARRAYTYPE_P(0);
    int nelems;
    double *nums = extract_numeric_array(array, &nelems);
    double result;

    qsort(nums, nelems, sizeof(double), compare_double);

    if (nelems % 2 == 1)
        result = nums[nelems / 2];
    else
        result = (nums[nelems / 2 - 1] + nums[nelems / 2]) / 2.0;

    if (!isfinite(result)) PG_RETURN_NULL();
    PG_RETURN_DATUM(double_to_numeric(result));
}


//дисперсия
PG_FUNCTION_INFO_V1(array_variance);

Datum
array_variance(PG_FUNCTION_ARGS)
{
    ArrayType *array = PG_GETARG_ARRAYTYPE_P(0);
    int nelems;
    double *nums = extract_numeric_array(array, &nelems);
    double mean;
    double result;

    if (nelems < 2)
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("Для вычисления дисперсии нужно минимум 2 элемента")));

    mean = calc_mean(nums, nelems);
    result = calc_sum_sq_diff(nums, nelems, mean) / (nelems - 1);

    if (!isfinite(result)) PG_RETURN_NULL();
    PG_RETURN_DATUM(double_to_numeric(result));
}


//стандартное отклонение
PG_FUNCTION_INFO_V1(array_stddev);

Datum
array_stddev(PG_FUNCTION_ARGS)
{
    ArrayType *array = PG_GETARG_ARRAYTYPE_P(0);
    int nelems;
    double *nums = extract_numeric_array(array, &nelems);
    double mean;
    double variance;
    double result;

    if (nelems < 2)
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("Для вычисления стандартного отклонения нужно минимум 2 элемента")));

    mean = calc_mean(nums, nelems);
    variance = calc_sum_sq_diff(nums, nelems, mean) / (nelems - 1);
    result = sqrt(variance);

    if (!isfinite(result)) PG_RETURN_NULL();
    PG_RETURN_DATUM(double_to_numeric(result));
}


//перцентиль 
PG_FUNCTION_INFO_V1(array_percentile);

Datum
array_percentile(PG_FUNCTION_ARGS)
{
    ArrayType *array = PG_GETARG_ARRAYTYPE_P(0);
    Numeric p_numeric = PG_GETARG_NUMERIC(1);
    int nelems;
    double *nums = extract_numeric_array(array, &nelems);
    double p;
    double index;
    double fraction;
    double result;
    int lower_idx;
    int upper_idx;

    p = DatumGetFloat8(DirectFunctionCall1(numeric_float8, NumericGetDatum(p_numeric)));

    if (!isfinite(p) || p < 0 || p > 100)
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("Перцентиль должен быть от 0 до 100")));

    qsort(nums, nelems, sizeof(double), compare_double);

    index = (p / 100.0) * (nelems - 1);
    
    if (index < 0) index = 0;
    if (index > nelems - 1) index = nelems - 1;

    lower_idx = (int) floor(index);
    upper_idx = lower_idx + 1;

    if (upper_idx >= nelems)
        upper_idx = nelems - 1;

    fraction = index - lower_idx;

    result = nums[lower_idx] + (nums[upper_idx] - nums[lower_idx]) * fraction;

    if (!isfinite(result)) PG_RETURN_NULL();
    PG_RETURN_DATUM(double_to_numeric(result));
}


//ковариация двух массивов
PG_FUNCTION_INFO_V1(array_cov);

Datum
array_cov(PG_FUNCTION_ARGS)
{
    ArrayType *arr1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType *arr2 = PG_GETARG_ARRAYTYPE_P(1);
    int n1, n2;
    double *nums1 = extract_numeric_array(arr1, &n1);
    double *nums2 = extract_numeric_array(arr2, &n2);
    double mean1;
    double mean2;
    double sum = 0;
    double result;
    int i;

    if (n1 != n2)
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("Массивы должны быть одинаковой длины")));

    if (n1 < 2)
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("Для вычисления ковариации нужно минимум 2 элемента")));

    mean1 = calc_mean(nums1, n1);
    mean2 = calc_mean(nums2, n2);

    for (i = 0; i < n1; i++)
        sum += (nums1[i] - mean1) * (nums2[i] - mean2);

    result = sum / (n1 - 1);

    if (!isfinite(result)) PG_RETURN_NULL();
    PG_RETURN_DATUM(double_to_numeric(result));
}


//коэффициент корреляции Пирсона
PG_FUNCTION_INFO_V1(array_corrcoef);

Datum
array_corrcoef(PG_FUNCTION_ARGS)
{
    ArrayType *arr1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType *arr2 = PG_GETARG_ARRAYTYPE_P(1);
    int n1, n2;
    double *nums1 = extract_numeric_array(arr1, &n1);
    double *nums2 = extract_numeric_array(arr2, &n2);
    double mean1;
    double mean2;
    double sum = 0;
    double ssq1;
    double ssq2;
    double result;
    int i;

    if (n1 != n2)
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("Массивы должны быть одинаковой длины")));

    if (n1 < 2)
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("Для вычисления корреляции нужно минимум 2 элемента")));

    mean1 = calc_mean(nums1, n1);
    mean2 = calc_mean(nums2, n2);

    for (i = 0; i < n1; i++)
        sum += (nums1[i] - mean1) * (nums2[i] - mean2);

    ssq1 = calc_sum_sq_diff(nums1, n1, mean1);
    ssq2 = calc_sum_sq_diff(nums2, n2, mean2);

    if (ssq1 == 0 || ssq2 == 0)
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("Корреляция не определена для массива с одинаковыми элементами")));

    result = sum / sqrt(ssq1 * ssq2);

    if (!isfinite(result)) PG_RETURN_NULL();
    PG_RETURN_DATUM(double_to_numeric(result));
}