# Paper Reproduction
Repo for all artifacts necessary to reproduce the results in the paper submission
**OLTP in the Cloud: Architectures, Tradeoffs, and Cost**.

## Build instructions
The repo contains a Makefile that allows the project to be built simply by running `make -j`.
You need clang++-16 on your system, which you can get by installing the latest LLVM from https://apt.llvm.org/
Alternatively, you can adapt the Makefile, but note that you need a fairly recent compiler.

## Usage instructions
Once you have successfully built the project, you can run `cloud_calc`.
For information about the accepted command line arguments, run `cloud_calc --help`:
```
Usage: ./cloud_calc --token <string> --vantage-csv <string> --datasize <uint64_t> --transactions <uint64_t> --update-ratio <double> --lookup-zipf <double> --max-replicas <uint64_t> --min-replicas <uint64_t> --pagesize <uint64_t> --tuplesize <uint64_t> --latency <uint64_t> --durability <unsigned> --[no-]group-commit --[no-]inter-az --sort <string> --priceunit <string> --instances <string> --architectures <string> --excludes <string> --delimiter <string> --[no-]trunc --[no-]filter --[no-]csv --[no-]show-hidden --[no-]hide-costs --[no-]hide-lookups --[no-]hide-updates --[no-]terse --intra-az-latency <double> --inter-az-latency <double>
  --token               vantage api token (default = )
  -c, --vantage-csv             vantage csv path (default = ./vantage.csv)
  --datasize            the size of the data set (in GB) (default = 100)
  --transactions                the number of operations (default = 10000)
  --update-ratio                the update ratio (default = 0.300000)
  --lookup-zipf         the skew of the keys of the lookup (default = 0.000000)
  --max-replicas                the max number of replicas for which to build architectures (default = 3)
  --min-replicas                the max number of replicas for which to build architectures (default = 0)
  --pagesize            the size of a single data page (default = 4096)
  --tuplesize           the size of a single tuple (default = 52)
  --latency             the required latency (in ns) for an operation (default = 9999999999)
  --durability          the required durability for an architecture (default = 0)
  --[no-]group-commit           let the model use group commit (default = 1)
  --[no-]inter-az               let the model try to distribute instances across AZs (default = 0)
  --sort                the category on which to sort (default = TotalPrice)
  --priceunit           print the prices by this unit (default = month)
  --instances           filter the instances by this (default = )
  --architectures               consider these architectures, empty=all (default = )
  --excludes            exclude these architectures (default = dynamic)
  --delimiter           delimiter for csv mode (default = ,)
  --[no-]trunc          truncate the results, but keep at least ten from each arch (default = 1)
  --[no-]filter         filter the results (default = 1)
  --[no-]csv            print in csv format (default = 0)
  --[no-]show-hidden            print metrics that are by default hidden (default = 0)
  --[no-]hide-costs             hide the costs (default = 0)
  --[no-]hide-lookups           hide the lookups (default = 0)
  --[no-]hide-updates           hide the updates (default = 0)
  --[no-]terse          hide the unimportant metrics (default = 0)
  --intra-az-latency            The assumed latency between two ec2 machines in the same AZ (default = 0.500000)
  --inter-az-latency            The assumed latency between two ec2 machines in different AZs in the same region (default = 1.000000)
```

In the following is an example input and the resulting output.
```
 % ./cloud_calc --datasize 1000 --transactions 10000 --update-ratio 0.3 --durability 4 --sort TotalPrice
num instances: 164
Create Classic architectures: 44
Create VBD architectures: 436
Create HADR architectures: 132
Create in-mem architectures: 9
Create Aurora architectures: 8528
Create Socrates architectures: 47996
Num assembled architectures: 0
 id|     Type|   Primary|CPUVendor|                   StorageDesc|St..|        LogDesc|numSec|Durabi..|OpLat..|Commi..|TotalPrice|PrimPrice|EBSPrice|SecPrice|LogSvcPrice|PageSvcPrice|S3Price|NetworkPrice|DataSize|PrimCache|PrimCacheHitrate|Storage|PrimLook..|SecLookups|  Lookups|Updates|PageReadVol|PageWriteVol|PrimNetIn|PrimNetOut|LogVolume|InterAZ|
  0| socrates|c5d.l-rb..|    intel|           0.018xi3en.24-rbpex| ec2| 4.2e-05xi4i.32|     0|   5x9's|215.3us|  292us|    0.3$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.2$/h| 0.0$/h|      0.0$/h|  1000gb|     49gb|           0.049|1000...|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   37.1mb|   293.0kb|  293.0kb|     0b|
  1| socrates|c5d.l-rb..|    intel|           0.018xi3en.24-rbpex| ec2|  9.6e-05xi3.16|     0|   5x9's|215.3us|  292us|    0.3$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.2$/h| 0.0$/h|      0.0$/h|  1000gb|     49gb|           0.049|1000...|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   37.1mb|   293.0kb|  293.0kb|     0b|
  2| socrates|c5d.l-rb..|    intel|           0.018xi3en.24-rbpex| ec2|4.6e-05xi3en.24|     0|   5x9's|215.3us|  292us|    0.3$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.2$/h| 0.0$/h|      0.0$/h|  1000gb|     49gb|           0.049|1000...|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   37.1mb|   293.0kb|  293.0kb|     0b|
  3| socrates|c5d.l-rb..|    intel|           0.018xi3en.24-rbpex| ec2| 0.00031xc5d.24|     0|   5x9's|215.3us|  292us|    0.3$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.2$/h| 0.0$/h|      0.0$/h|  1000gb|     49gb|           0.049|1000...|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   37.1mb|   293.0kb|  293.0kb|     0b|
  4| socrates|c5d.l-rb..|    intel|           0.018xi3en.24-rbpex| ec2| 0.00031xm5d.24|     0|   5x9's|215.3us|  292us|    0.3$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.2$/h| 0.0$/h|      0.0$/h|  1000gb|     49gb|           0.049|1000...|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   37.1mb|   293.0kb|  293.0kb|     0b|
  5| socrates|c5d.l-rb..|    intel|           0.018xi3en.24-rbpex| ec2|0.00031xm5dn.24|     0|   5x9's|215.3us|  292us|    0.3$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.2$/h| 0.0$/h|      0.0$/h|  1000gb|     49gb|           0.049|1000...|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   37.1mb|   293.0kb|  293.0kb|     0b|
  6| socrates|c5d.l-rb..|    intel|           0.018xi3en.24-rbpex| ec2| 0.00031xr5d.24|     0|   5x9's|215.3us|  292us|    0.3$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.2$/h| 0.0$/h|      0.0$/h|  1000gb|     49gb|           0.049|1000...|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   37.1mb|   293.0kb|  293.0kb|     0b|
  7| socrates|c5d.l-rb..|    intel|           0.018xi3en.24-rbpex| ec2|0.00031xr5dn.24|     0|   5x9's|215.3us|  292us|    0.3$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.2$/h| 0.0$/h|      0.0$/h|  1000gb|     49gb|           0.049|1000...|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   37.1mb|   293.0kb|  293.0kb|     0b|
  8| socrates|c5d.l-rb..|    intel|           0.018xi3en.24-rbpex| ec2| 0.00062xz1d.12|     0|   5x9's|215.3us|  292us|    0.3$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.2$/h| 0.0$/h|      0.0$/h|  1000gb|     49gb|           0.049|1000...|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   37.1mb|   293.0kb|  293.0kb|     0b|
  9| socrates|c5d.l-rb..|    intel|           0.018xi3en.24-rbpex| ec2|0.00029xx2idn..|     0|   5x9's|215.3us|  292us|    0.3$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.2$/h| 0.0$/h|      0.0$/h|  1000gb|     49gb|           0.049|1000...|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   37.1mb|   293.0kb|  293.0kb|     0b|
 10|     hadr|    i3en.l|    intel|1.2tb(nvme;42.5k r/s;32.5k w..|nvme|      inst-stor|     1|   5x9's|129.9us|   44us|    0.5$/h|   0.2$/h|  0.0$/h|  0.2$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     16gb|           0.016| 1000gb|  7000.0/s|     0.0/s| 7000.0/s|3000...|     38.4mb|      11.5mb|       0b|   445.3kb|  445.3kb|     0b|
 11|     hadr|    i3en.l|    intel|1.2tb(nvme;42.5k r/s;32.5k w..|nvme|      inst-stor|     2|   9x9's|129.9us|   44us|    0.7$/h|   0.2$/h|  0.0$/h|  0.5$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     16gb|           0.016| 1000gb|  7000.0/s|     0.0/s| 7000.0/s|3000...|     25.0mb|       7.5mb|       0b|   890.6kb|  445.3kb|     0b|
 12|   aurora|      c5.l|    intel|       comb-p+l(0.056xi3en.24)|    |comb-p+l(0.05..|     0|      20|215.5us|  160us|    0.7$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.6$/h| 0.0$/h|      0.0$/h|  1000gb|      4gb|           0.004|  2.9tb|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   38.9mb|     1.7mb|  293.0kb|     0b|
 13|   aurora|     c5d.l|    intel|       comb-p+l(0.056xi3en.24)|    |comb-p+l(0.05..|     0|      20|215.5us|  160us|    0.7$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.6$/h| 0.0$/h|      0.0$/h|  1000gb|      4gb|           0.004|  2.9tb|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   38.9mb|     1.7mb|  293.0kb|     0b|
 14|   aurora|      m5.l|    intel|       comb-p+l(0.056xi3en.24)|    |comb-p+l(0.05..|     0|      20|214.7us|  160us|    0.7$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.6$/h| 0.0$/h|      0.0$/h|  1000gb|      8gb|           0.008|  2.9tb|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   38.8mb|     1.7mb|  293.0kb|     0b|
 15|   aurora|     c5n.l|    intel|       comb-p+l(0.056xi3en.24)|    |comb-p+l(0.05..|     0|      20|215.2us|  160us|    0.7$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.6$/h| 0.0$/h|      0.0$/h|  1000gb|    5.2gb|         0.00525|  2.9tb|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   38.9mb|     1.7mb|  293.0kb|     0b|
 16|   aurora|     m5d.l|    intel|       comb-p+l(0.056xi3en.24)|    |comb-p+l(0.05..|     0|      20|214.7us|  160us|    0.7$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.6$/h| 0.0$/h|      0.0$/h|  1000gb|      8gb|           0.008|  2.9tb|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   38.8mb|     1.7mb|  293.0kb|     0b|
 17|   aurora|     m5n.l|    intel|       comb-p+l(0.056xi3en.24)|    |comb-p+l(0.05..|     0|      20|214.7us|  160us|    0.7$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.6$/h| 0.0$/h|      0.0$/h|  1000gb|      8gb|           0.008|  2.9tb|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   38.8mb|     1.7mb|  293.0kb|     0b|
 18|   aurora|      r5.l|    intel|       comb-p+l(0.056xi3en.24)|    |comb-p+l(0.05..|     0|      20|212.9us|  160us|    0.7$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.6$/h| 0.0$/h|      0.0$/h|  1000gb|     16gb|           0.016|  2.9tb|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   38.4mb|     1.7mb|  293.0kb|     0b|
 19|   aurora|      r4.l|    intel|       comb-p+l(0.056xi3en.24)|    |comb-p+l(0.05..|     0|      20|213.1us|  160us|    0.7$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.6$/h| 0.0$/h|      0.0$/h|  1000gb|   15.2gb|         0.01525|  2.9tb|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   38.5mb|     1.7mb|  293.0kb|     0b|
 20|   aurora|    m5dn.l|    intel|       comb-p+l(0.056xi3en.24)|    |comb-p+l(0.05..|     0|      20|214.7us|  160us|    0.7$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.6$/h| 0.0$/h|      0.0$/h|  1000gb|      8gb|           0.008|  2.9tb|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   38.8mb|     1.7mb|  293.0kb|     0b|
 21|   aurora|     r5d.l|    intel|       comb-p+l(0.056xi3en.24)|    |comb-p+l(0.05..|     0|      20|212.9us|  160us|    0.7$/h|   0.1$/h|  0.0$/h|  0.0$/h|     0.0$/h|      0.6$/h| 0.0$/h|      0.0$/h|  1000gb|     16gb|           0.016|  2.9tb|  7000.0/s|     0.0/s| 7000.0/s|3000...|         0b|          0b|   38.4mb|     1.7mb|  293.0kb|     0b|
 22|     hadr|    i3en.l|    intel|1.2tb(nvme;42.5k r/s;32.5k w..|nvme|      inst-stor|     3|      20|129.9us|   44us|    0.9$/h|   0.2$/h|  0.0$/h|  0.7$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     16gb|           0.016| 1000gb|  7000.0/s|     0.0/s| 7000.0/s|3000...|     20.5mb|       6.2mb|       0b|     1.3mb|  445.3kb|     0b|
 23|     hadr|   i3en.xl|    intel|   2.4tb(nvme;85k r/s;65k w/s)|nvme|      inst-stor|     1|   5x9's|127.8us|   44us|    0.9$/h|   0.5$/h|  0.0$/h|  0.5$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     32gb|           0.032| 1000gb|  7000.0/s|     0.0/s| 7000.0/s|3000...|     37.8mb|      11.3mb|       0b|   445.3kb|  445.3kb|     0b|
 24|     hadr|      i3.2|    intel|1.9tb(nvme;412.5k r/s;180k w..|nvme|      inst-stor|     1|   5x9's|124.0us|   44us|    1.2$/h|   0.6$/h|  0.0$/h|  0.6$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     61gb|           0.061| 1000gb|  7000.0/s|     0.0/s| 7000.0/s|3000...|     36.7mb|      11.0mb|       0b|   445.3kb|  445.3kb|     0b|
 25|     hadr|   i3en.xl|    intel|   2.4tb(nvme;85k r/s;65k w/s)|nvme|      inst-stor|     2|   9x9's|127.8us|   44us|    1.4$/h|   0.5$/h|  0.0$/h|  0.9$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     32gb|           0.032| 1000gb|  7000.0/s|     0.0/s| 7000.0/s|3000...|     24.6mb|       7.4mb|       0b|   890.6kb|  445.3kb|     0b|
 26|     hadr|     i4i.2|    intel| 1.8tb(nvme;200k r/s;110k w/s)|nvme|      inst-stor|     1|   5x9's|123.6us|   44us|    1.4$/h|   0.7$/h|  0.0$/h|  0.7$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     64gb|           0.064| 1000gb|  7000.0/s|     0.0/s| 7000.0/s|3000...|     36.6mb|      11.0mb|       0b|   445.3kb|  445.3kb|     0b|
 27|     hadr|    i3en.2|    intel|2x2.4tb(nvme;170k r/s;130k w..|nvme|      inst-stor|     1|   5x9's|123.6us|   44us|    1.8$/h|   0.9$/h|  0.0$/h|  0.9$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     64gb|           0.064| 1000gb|  7000.0/s|     0.0/s| 7000.0/s|3000...|     36.6mb|      11.0mb|       0b|   445.3kb|  445.3kb|     0b|
 28|     hadr|   i3en.xl|    intel|   2.4tb(nvme;85k r/s;65k w/s)|nvme|      inst-stor|     3|      20|127.8us|   44us|    1.8$/h|   0.5$/h|  0.0$/h|  1.4$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     32gb|           0.032| 1000gb|  7000.0/s|     0.0/s| 7000.0/s|3000...|     20.2mb|       6.1mb|       0b|     1.3mb|  445.3kb|     0b|
 29|     hadr|      i3.2|    intel|1.9tb(nvme;412.5k r/s;180k w..|nvme|      inst-stor|     2|   9x9's|124.0us|   44us|    1.9$/h|   0.6$/h|  0.0$/h|  1.2$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     61gb|           0.061| 1000gb|  7000.0/s|     0.0/s| 7000.0/s|3000...|     23.8mb|       7.2mb|       0b|   890.6kb|  445.3kb|     0b|
 30|      rbd|     r5b.2|    intel|io2(1001.5gb;12.2kop/s;50.3m..| io2|io2(1001.5gb;..|     0|   5x9's|350.1us|  292us|    1.9$/h|   0.6$/h|  1.3$/h|  0.0$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     64gb|           0.064|1001...|  7000.0/s|     0.0/s| 7000.0/s|3000...|     47.5mb|      47.5mb|       0b|        0b|  445.3kb|     0b|
 31|      rbd|    m5zn.2|    intel|io2(1001.5gb;12.6kop/s;52.0m..| io2|io2(1001.5gb;..|     0|   5x9's|362.0us|  292us|    2.0$/h|   0.7$/h|  1.3$/h|  0.0$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     32gb|           0.032|1001...|  7000.0/s|     0.0/s| 7000.0/s|3000...|     49.2mb|      49.2mb|       0b|        0b|  445.3kb|     0b|
 32|      rbd|      c5.4|    intel|io2(1001.5gb;12.6kop/s;52.0m..| io2|io2(1001.5gb;..|     0|   5x9's|362.0us|  292us|    2.0$/h|   0.7$/h|  1.3$/h|  0.0$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     32gb|           0.032|1001...|  7000.0/s|     0.0/s| 7000.0/s|3000...|     49.2mb|      49.2mb|       0b|        0b|  445.3kb|     0b|
 33|      rbd|     z1d.2|    intel|io2(1001.5gb;12.2kop/s;50.3m..| io2|io2(1001.5gb;..|     0|   5x9's|350.1us|  292us|    2.0$/h|   0.7$/h|  1.3$/h|  0.0$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     64gb|           0.064|1001...|  7000.0/s|     0.0/s| 7000.0/s|3000...|     47.5mb|      47.5mb|       0b|        0b|  445.3kb|     0b|
 34|      rbd|      m5.4|    intel|io2(1001.5gb;12.2kop/s;50.3m..| io2|io2(1001.5gb;..|     0|   5x9's|350.1us|  292us|    2.1$/h|   0.8$/h|  1.3$/h|  0.0$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     64gb|           0.064|1001...|  7000.0/s|     0.0/s| 7000.0/s|3000...|     47.5mb|      47.5mb|       0b|        0b|  445.3kb|     0b|
 35|      rbd|     c5d.4|    intel|io2(1001.5gb;12.6kop/s;52.0m..| io2|io2(1001.5gb;..|     0|   5x9's|362.0us|  292us|    2.1$/h|   0.8$/h|  1.3$/h|  0.0$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     32gb|           0.032|1001...|  7000.0/s|     0.0/s| 7000.0/s|3000...|     49.2mb|      49.2mb|       0b|        0b|  445.3kb|     0b|
 36|      rbd|     c5n.4|    intel|io2(1001.5gb;12.5kop/s;51.5m..| io2|io2(1001.5gb;..|     0|   5x9's|358.3us|  292us|    2.2$/h|   0.9$/h|  1.3$/h|  0.0$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     42gb|           0.042|1001...|  7000.0/s|     0.0/s| 7000.0/s|3000...|     48.7mb|      48.7mb|       0b|        0b|  445.3kb|     0b|
 37|      rbd|     m5d.4|    intel|io2(1001.5gb;12.2kop/s;50.3m..| io2|io2(1001.5gb;..|     0|   5x9's|350.1us|  292us|    2.2$/h|   0.9$/h|  1.3$/h|  0.0$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     64gb|           0.064|1001...|  7000.0/s|     0.0/s| 7000.0/s|3000...|     47.5mb|      47.5mb|       0b|        0b|  445.3kb|     0b|
 38|      rbd|      r5.4|    intel|io2(1001.5gb;11.3kop/s;46.9m..| io2|io2(1001.5gb;..|     0|   5x9's|326.2us|  292us|    2.2$/h|   1.0$/h|  1.2$/h|  0.0$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|    128gb|           0.128|1001...|  7000.0/s|     0.0/s| 7000.0/s|3000...|     44.3mb|      44.3mb|       0b|        0b|  445.3kb|     0b|
 39|      rbd|     m5n.4|    intel|io2(1001.5gb;12.2kop/s;50.3m..| io2|io2(1001.5gb;..|     0|   5x9's|350.1us|  292us|    2.2$/h|   1.0$/h|  1.3$/h|  0.0$/h|     0.0$/h|      0.0$/h| 0.0$/h|      0.0$/h|  1000gb|     64gb|           0.064|1001...|  7000.0/s|     0.0/s| 7000.0/s|3000...|     47.5mb|      47.5mb|       0b|        0b|  445.3kb|     0b|
```

## Examples to reproduce experiments in the paper
### Figure 3
`for TX in {1000,10000,100000,1000000,10000000,100000000}; do for DATASIZE in {10,100,1000,10000,100000}; do ./cloud_calc --datasize ${DATASIZE} --transactions ${TX} --update-ratio 0.3 --durability 4 --sort TotalPrice; done; done`

### Figure 7
` for SKEW in {0.0,0.5,1.0,1.5,2.0}; do ./cloud_calc --datasize 1000 --transactions 1000000 --update-ratio 0.0 --durability 1 --lookup-zipf ${SKEW} --sort TotalPrice; done`
