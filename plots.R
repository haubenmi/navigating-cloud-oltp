library(ggplot2)
library(reshape2)
library(ggthemes)
library(stringr)
library(sqldf)
library(doMC)
library(dplyr)
library(RPostgreSQL)
library(plotwidgets)

hsv2col <- function(hsvValue, ...)
{
   ## Purpose is to augment the hsv() function which does not handle
   ## output from rgb2hsv(). It should be possible to run hsv2col(rgb2hsv(x)).
   ##
   ## This function also handles alpha.
   ##
   ## This function also allows value above 1, which have the effect of reducing
   ## the saturation.
   if (all(is.null(dim(hsvValue)))) {
      do.call(hsv, hsvValue, ...);
   } else {
      if (!"alpha" %in% rownames(hsvValue)) {
         hsvValue <- rbind(hsvValue,
            matrix(nrow=1, rep(1, ncol(hsvValue)),
               dimnames=c("alpha",list(colnames(hsvValue)))));
      }
      hsv(h=hsvValue["h",],
         s=hsvValue["s",],
         v=hsvValue["v",],
         alpha=hsvValue["alpha",]);
   }
}
########################################
baseDir <- "~/"
options(cores = 10)

options("width"=200)

options(sqldf.RPostgreSQL.user = "ruser",
  sqldf.RPostgreSQL.password = "",
  sqldf.RPostgreSQL.dbname = "rdb",
  sqldf.RPostgreSQL.host = "localhost",
  sqldf.RPostgreSQL.port = 9011)
Sys.setlocale("LC_MESSAGES", 'en_GB.UTF-8')
Sys.setenv(LANG = "en_US.UTF-8")

registerDoMC()
csvDelimiter <- ","
########################################
formatterGB <- function(x) { as.numeric(x)/1024/1024/1024 }
formatterTB <- function(x) { as.numeric(x)/1024/1024/1024/1000 }
formatterPerc <- function(x) { sprintf("%s%", x) }
scientific_10 <- function(x) {
    tmp<- gsub("1,x,", "",gsub("e\\+", ",x,10^",scales::scientific_format()(as.numeric(x))))
    tmp <- paste0("paste(",tmp,")")
    parse(text=tmp)
}
runCalc <- function(datasetSize, requiredOps, lookupZipf, percentUpdates, requiredLatency, requiredDurability, interAz = FALSE, noGroupCommit = FALSE, minReplica = 0, maxReplica = 3) {
   cloud_calc_cmd <- paste(paste0(baseDir, "./cloud_calc"),
                    "--datasize", datasetSize,
                    "--transactions", requiredOps,
                    "--update-ratio", (percentUpdates / 100),
                    "--lookup-zipf", lookupZipf,
                    "--min-replicas", minReplica,
                    "--max-replicas", maxReplica,
                    "--latency", requiredLatency,
                    "--page-server-replication", 2,
                    "--durability", requiredDurability,
                    "--priceunit", "hour",
                    "--csv",
                    (if (noGroupCommit) "--no-group-commit" else ""),
                    "--filter",
                    (if (interAz) "--inter-az" else ""),
                    "--trunc", 5,
                    "--excludes", "dynamic",
                    "--sort", "TotalPrice,-Durability,OpLatency,numSec,Type",
                    "--delimiter", csvDelimiter
                    )
   print(cloud_calc_cmd)
   out <- system(cloud_calc_cmd, intern = TRUE)
   data <- read.table(text = out, sep = csvDelimiter, header = TRUE)
   data
}

reduceSat <- function(x) {
    y <- rgb2hsv(col2rgb(x))
    y["s",] <- max(0,y["s",] - 0.15)
    hsv2col(y)
}

architectureColors <- c("classic" = reduceSat(rgb(174,199,232, maxColorValue = 255)),
                        "hadr" = reduceSat(rgb(137,87,165, maxColorValue = 255)),
                        "rbd" = rgb(43,131,186, maxColorValue = 255),
                        "aurora" = rgb(127,191,123, maxColorValue = 255),
                        "socrates" = rgb(253,174,97, maxColorValue = 255),
                        "inmem" = rgb(241,101,162, maxColorValue = 255)
                        )
architectureShapes <- c("classic" = 15,
                        "hadr" = 19,
                        "rbd" = 17,
                        "aurora" = 3,
                        "socrates" = 5,
                        "inmem" = 4
                        )
datasetLabels = c(
    "1073741824" =      "1GB  ",
    "10737418240" =     "10GB ",
    "107374182400" =    "100GB",
    "1073741824000" =   "1TB  ",
    "10737418240000" =  "10TB ",
    "107374182400000" = "100TB"
)
archNamesBest <- c(
    "classic" = "Classic vs. Best",
    "hadr" = "HADR vs. Best",
    "rbd" = "RBD vs. Best",
    "aurora" = "Aurora vs. Best",
    "socrates" = "Socrates vs. Best",
    "inmem" = "In-Memory vs. Best"
)
archNames <- c(
    "classic" = "Classic",
    "hadr" = "HADR",
    "rbd" = "RBD",
    "aurora" = "Aurora-like",
    "socrates" = "Socrates-like",
    "inmem" = "In-Memory"
)
archLevels <- c('classic','hadr','rbd','aurora','socrates','inmem')

runAllCalcs <- function(ops, lookupZipf, updates,latency,dataset,durabilities, interAz, noGroupCommit = FALSE, minReplica = 0, maxReplica = 3) {
    totalCombinations <- length(ops) * length(updates) * length(latency) * length(dataset) * length(durabilities) * length(lookupZipf) * length(interAz)
    print(paste0("Total combinations: ", totalCombinations))
    offset <- 0
    options(scipen=999)
    finalData <- data.frame()
    for (requiredOps in ops) {
        for (percentUpdates in updates) {
            for (requiredLatency in latency) {
                for (requiredDurability in durabilities) {
                        for (zipf in lookupZipf) {
                          for (interA in interAz) {
                              fd=list()
                              for (i in 1:length(dataset)) {
                                  data <- runCalc(dataset[i], requiredOps, zipf, percentUpdates, requiredLatency, requiredDurability, interA, noGroupCommit, minReplica, maxReplica)
                                  if (nrow(data) > 0) {
                                      data$ops = requiredOps
                                      data$lookupZipf = zipf
                                      data$percentUpdates = percentUpdates
                                      data$requiredLatency = requiredLatency
                                      data$requiredDurability = requiredDurability
                                      data$interAZ = interA
                                  }
                                  fd <- append(fd,list(data))
                                  print(paste0("Processing combination ",offset," of ",totalCombinations))
                                  offset <- offset+1
                              }
                              tmp <-data.frame()
                              for (a in fd) {
                                  if (nrow(a) > 0) {
                                      tmp <- bind_rows(tmp,a)
                                  }
                              }
                              finalData <- bind_rows(finalData,tmp)
                              print(paste0("currentSize: ", dim(finalData)))
                          }
                          }
                }
            }
        }
    }
    finalData
}



########################################
## Best instace for reads/datasize; color architecture type
opsVector <- c(1000,10000,100000,1000000,10000000,100000000)
skewVector <- c(0.0)
updates <- 30
percentUpdatesVector <- c(updates)#,30,100)
datasetSizes <- c(10,100,1000,10000,100000)
latencyLevels <- c(1000000)#c(1000,10000,100000,1000000)
durability <- 3
durabilityLevels <- c(durability)#,4,7,11)
interAZ <- c(FALSE)
noGroupCommit <- FALSE
generic_wl_data_raw <- runAllCalcs(opsVector,skewVector, percentUpdatesVector,latencyLevels,datasetSizes, durabilityLevels, interAZ, noGroupCommit)

generic_wl_data <- sqldf(paste0('WITH combinations AS (SELECT ops,"DataSize","percentUpdates","requiredDurability","requiredLatency" FROM "generic_wl_data_raw" GROUP BY ops,"DataSize","percentUpdates","requiredDurability", "requiredLatency"),
bestIds AS (
  SELECT cd.*,(SELECT fd.id
               FROM "generic_wl_data_raw" fd
               WHERE fd.ops=cd.ops AND fd."percentUpdates"=cd."percentUpdates" AND fd."DataSize"=cd."DataSize" AND fd."requiredDurability"=cd."requiredDurability" AND fd."requiredLatency"=cd."requiredLatency" AND fd."Type"!=\'dynamic\'
               ORDER BY fd."TotalPrice" ASC, fd."Durability" DESC, fd."OpLatency" ASC, "numSec" ASC, fd."Type" DESC, fd.id ASC LIMIT 1) as bestId FROM combinations cd)
SELECT (CASE WHEN fd."numSec" > 0 THEN (fd."numSec"+1)::text || \'×\' ELSE \'\' END) || replace(fd."Primary",\'-rbpex\',\'\') || \'\n$\' || (case when fd."TotalPrice">=9.9 THEN (fd."TotalPrice"::int)::text when fd."TotalPrice">=0.95 THEN round(fd."TotalPrice"::numeric(10,1),1)::text ELSE round(fd."TotalPrice"::numeric(10,2),2)::text END) as "NamePrice",
    (fd."numSec" + 1) as "numInstances",
    fd."Type",
    fd."TotalPrice",
    upper(substr(fd."Type",0,2)) as flett,
    fd."DataSize", fd.ops, fd."percentUpdates" FROM "generic_wl_data_raw" fd, bestIds bi WHERE bi.ops=fd.ops AND bi."percentUpdates"=fd."percentUpdates" AND bi."DataSize"=fd."DataSize" AND bi."requiredLatency"=fd."requiredLatency" AND bi."requiredDurability"=fd."requiredDurability" AND fd.id=bi.bestId;'))

generic_wl_data$myPercent = ordered(generic_wl_data$percentUpdates,levels=c(paste0(updates)),labels=c(paste0(updates,'% Updates; ',durability,'x9\'s Durability')))
plot1 <- ggplot(generic_wl_data, aes(x=factor(DataSize),y=factor(ops),fill=Type)) +
    scale_fill_manual(values = architectureColors, label=archNames) +
    geom_tile(color="black") +
    geom_text(mapping=aes(label=flett), size=3, alpha=0.25, position=position_nudge(x=-0.38, y=-0.30)) +
    geom_text(mapping=aes(label=NamePrice), size=3.1) +
    scale_x_discrete(name = "Dataset Size", label = datasetLabels, expand=c(0,0)) +
    scale_y_discrete(name = "Operations / sec", labels=scientific_10, expand=c(0,0)) +
    facet_wrap(vars(myPercent), nrow = 1) +
    theme_bw() +
  theme(
    axis.text.x=element_text(size=12),
    axis.text.y=element_text(size=14),
    axis.ticks = element_blank(),
    axis.title=element_text(size=18),
    panel.grid.major = element_blank(),
    panel.grid.minor = element_blank(),
    strip.text.x = element_text(size = 14.0),
    legend.title = element_blank(),
    legend.position = "top",
    legend.background = element_blank(),
    legend.justification = c(0.5,0.5),
    legend.key.size = unit(7.5, "mm"),
    legend.text = element_text(size=16),
    legend.spacing.y = unit(5, "mm"),
    legend.spacing.x = unit(2, "mm")
  ) + guides(fill = guide_legend(nrow = 1))
ggsave(plot = plot1, filename = "generic_wl_part1.pdf", device = cairo_pdf, width = 3.8, height = 3.7)
########################################


# Compare architecture to best instance
bestPriceData <- sqldf(paste0('
WITH combinationsWithType AS (SELECT ops,"DataSize","percentUpdates","Type","requiredDurability","requiredLatency" FROM "generic_wl_data_raw" GROUP BY 1,2,3,4,5,6),
combinations AS (
      SELECT ops,"DataSize","percentUpdates","requiredDurability","requiredLatency"
      FROM combinationsWithType
      GROUP BY 1,2,3,4,5),
bestPriceForType AS (
      SELECT cd.*,(SELECT fd.id
                   FROM "generic_wl_data_raw" fd
                   WHERE fd.ops=cd.ops AND fd."percentUpdates"=cd."percentUpdates" AND fd."DataSize"=cd."DataSize" AND fd."Type"=cd."Type" AND fd."requiredDurability"=cd."requiredDurability" AND fd."requiredLatency"=cd."requiredLatency"
                   ORDER BY fd."TotalPrice" ASC, fd."Durability" DESC, fd."OpLatency" ASC, "numSec" ASC, fd.id ASC LIMIT 1) as bestId FROM combinationsWithType cd),
bestPrice AS (
    SELECT cd.*,(
        SELECT fd."TotalPrice"
        FROM "generic_wl_data_raw" fd
        WHERE fd.ops=cd.ops AND fd."percentUpdates"=cd."percentUpdates" AND fd."DataSize"=cd."DataSize" AND fd."requiredLatency"=cd."requiredLatency" AND fd."requiredDurability"=cd."requiredDurability"
        ORDER BY fd."TotalPrice" ASC, fd."Durability" DESC, fd."OpLatency" ASC, "numSec" ASC, fd.id ASC LIMIT 1) as bestPrice FROM combinations cd)
SELECT fd."Primary",
       fd.ops,
       fd."percentUpdates",
       fd."Type",
       fd."DataSize",
       CASE WHEN fd."TotalPrice"/bp.bestPrice >= 10 THEN ((fd."TotalPrice"/bp.bestPrice)::int) ELSE round(fd."TotalPrice"/bp.bestPrice::numeric(15,10),1) END as "priceRatio",
       CASE WHEN fd."TotalPrice"/bp.bestPrice <= 1.05 THEN \'1\' WHEN fd."TotalPrice"/bp.bestPrice >= 10 THEN ((fd."TotalPrice"/bp.bestPrice)::int)::text ELSE round((fd."TotalPrice"/bp.bestPrice)::numeric(10,1),1)::text END as "priceRatioText",
       5*log(fd."TotalPrice"/bp.bestPrice - 0.5) as "colorRatio",
       bp.bestPrice
       FROM "generic_wl_data_raw" fd, bestPriceForType bpft, bestPrice bp
       WHERE fd."percentUpdates"=bpft."percentUpdates" AND fd."DataSize"=bpft."DataSize" AND fd."Type"=bpft."Type" AND fd.ops=bpft.ops AND fd.id=bpft.bestId AND fd."requiredLatency"=bpft."requiredLatency" AND fd."requiredDurability"=bpft."requiredDurability"
AND bp.ops=fd.ops AND bp."percentUpdates"=fd."percentUpdates" AND bp."DataSize"=fd."DataSize" AND fd."requiredLatency"=bp."requiredLatency" AND fd."requiredDurability"=bp."requiredDurability"'))

bestPriceData$competitor = factor(bestPriceData$Type, levels=archLevels, labels=archNamesBest)
plot <- ggplot(bestPriceData, aes(x=factor(DataSize),y=factor(ops),fill=priceRatio)) +
scale_fill_gradientn(colours=c(rgb(48,147,67,maxColorValue=255),rgb(255,221,113,maxColorValue=255),rgb(216,37,38,maxColorValue=255),rgb(216,37,38,maxColorValue=300)), values=c(0,0.055,0.3,1)) +
    geom_tile(color="black") +
    geom_text(mapping=aes(label=priceRatioText), size=4.7) +
    scale_x_discrete(name = "Dataset Size", label = datasetLabels, expand=c(0,0)) +
    scale_y_discrete(name = "", labels=scientific_10, expand=c(0,0)) +
    facet_grid(cols=vars(competitor),rows=vars(percentUpdates)) +
    theme_bw() +
    theme(
        axis.text.x=element_blank(),
        axis.text.y=element_blank(),
        axis.ticks.x=element_blank(),
        axis.ticks.y=element_blank(),
        panel.grid.major = element_blank(),
        panel.grid.minor = element_blank(),
        axis.title=element_blank(),
        strip.text.y = element_blank(),
        strip.text.x = element_text(size = 14.0),
        legend.position = "none"
    )
ggsave(plot = plot, filename = "generic_wl_part2.pdf", device = cairo_pdf, width = 7.5, height = 2.62)
########################################

########################################
### Durability
########################################
opsVector <- c(1000,10000,100000,1000000,10000000,100000000)
skewVector <- c(0.0)
percentUpdatesVector <- c(30)#,30,100)
datasetSizes <- c(10,100,1000,10000,100000)
latencyLevels <- c(1000000)#c(1000,10000,100000,1000000)
durabilityLevels <- c(1,3,5,11)#,4,7,11)
interAZ <- c(FALSE)
noGroupCommit <- FALSE
durabilityRawData <- runAllCalcs(opsVector,skewVector, percentUpdatesVector,latencyLevels,datasetSizes, durabilityLevels, interAZ, noGroupCommit)

durabilityData <- sqldf(paste0('WITH
combinations AS (SELECT ops,"DataSize","percentUpdates","requiredDurability" FROM "durabilityRawData" GROUP BY ops,"DataSize","percentUpdates","requiredDurability"),
bestIds AS (SELECT cd.*,(SELECT fd.id FROM "durabilityRawData" fd WHERE fd.ops=cd.ops AND fd."percentUpdates"=cd."percentUpdates" AND fd."DataSize"=cd."DataSize" AND fd."Durability">=cd."requiredDurability" ORDER BY fd."TotalPrice" ASC, fd."Durability" DESC, fd."OpLatency" ASC, "numSec" ASC, fd."Type" ASC, fd.id ASC LIMIT 1) as bestId FROM combinations cd)
SELECT bi."requiredDurability" as dura,
      \'$\' || (case when fd."TotalPrice">=9.9 THEN (fd."TotalPrice"::int)::text when fd."TotalPrice">=0.95 THEN round(fd."TotalPrice"::numeric(10,1),1)::text ELSE round(fd."TotalPrice"::numeric(10,2),2)::text END) as "NamePrice",
      upper(substr(fd."Type",0,2)) as flett,
      fd."numSec",
      fd."Type",
      fd."TotalPrice",
      fd."DataSize", fd.ops, fd."percentUpdates"
      FROM "durabilityRawData" fd, bestIds bi
      WHERE bi.ops=fd.ops AND bi."percentUpdates"=fd."percentUpdates" AND bi."DataSize"=fd."DataSize" AND bi."requiredDurability"=fd."requiredDurability" AND fd.id=bi.bestId;'))
head(durabilityData)

########################################
durabilityData$myDura <- ordered(durabilityData$dura, levels=c(0,1,3,4,5,7,11), labels=c("No Durability Required","≥ 1×9's Durability","≥ 3×9's Durability","≥ 4×9's Durability","≥ 5×9's Durability","≥ 7×9's Durability", "≥ 11×9's Durability")) # Multi-AZ Deploy. +
plot <- ggplot(durabilityData, aes(x=factor(DataSize),y=factor(ops),fill=Type)) +
    scale_fill_manual(values = architectureColors, labels=archNames) +
    geom_tile(color="black") +
    geom_text(mapping=aes(label=flett), size=3, alpha=0.25, position=position_nudge(x=-0.37, y=-0.00)) +
    geom_text(mapping=aes(label=NamePrice), size=3.2, position=position_nudge(x=0.1,y=0.00)) +
    scale_x_discrete(name = "Dataset Size", label = datasetLabels, expand=c(0,0)) +
    scale_y_discrete(name = "Operations / sec", label = scientific_10, expand=c(0,0)) +
    facet_wrap(vars(myDura), nrow = 1) +
    theme_bw() +
    theme(
    axis.text.x=element_text(size=9.5, hjust=0.4,vjust=1.2),
    axis.text.y=element_text(size=11,hjust=0.2),
    axis.ticks = element_blank(),
    axis.title=element_text(size=18),
    panel.grid.major = element_blank(),
    panel.grid.minor = element_blank(),
    strip.text.x = element_text(size = 15.0),
    legend.title = element_blank(),
    legend.position = "top",
    legend.background = element_blank(),
    legend.justification = c(0.5,0.5),
    legend.key.size = unit(7.5, "mm"),
#    legend.key = element_blank()#element_rect(color="transparent"),
    legend.text = element_text(size=16),
    legend.spacing.y = unit(5, "mm"),
    legend.spacing.x = unit(2, "mm"),
    ) + guides(fill = guide_legend(nrow = 1))
ggsave(plot = plot, filename = "durability.pdf", device = cairo_pdf, width = 9.4, height = 2.95) # 4.4 width for one
########################################

head(durabilityRawData)

durabilityRatioData <- sqldf(paste0('WITH
combinations AS (SELECT ops,"DataSize","requiredDurability" FROM "durabilityRawData" GROUP BY 1,2,3),
bestIds AS (SELECT cd.*,(SELECT fd.id FROM "durabilityRawData" fd WHERE fd.ops=cd.ops AND fd."DataSize"=cd."DataSize" AND fd."requiredDurability">=cd."requiredDurability" ORDER BY fd."TotalPrice" ASC, fd."Durability" DESC, fd."OpLatency" ASC, "numSec" ASC, fd."Type" ASC, fd.id ASC LIMIT 1) as bestId FROM combinations cd),
basePrice AS (SELECT bi.*,fd."TotalPrice" as price FROM bestIds bi, "durabilityRawData" fd WHERE bi."requiredDurability"=1 AND bi.ops=fd.ops AND bi."DataSize"=fd."DataSize" AND bi.bestId=fd.id AND bi."requiredDurability"=fd."requiredDurability"),
result AS (SELECT fd."Type",fd."requiredDurability" as dura,fd.ops, fd."DataSize",fd."TotalPrice",
       CASE WHEN fd."TotalPrice"/ba.price >= 10 THEN ((fd."TotalPrice"/ba.price)::int) ELSE round(fd."TotalPrice"/ba.price::numeric(15,10),1) END as "priceRatio",
       CASE WHEN (fd."TotalPrice"/ba.price <= 1.05 AND fd."TotalPrice"/ba.price >= 0.95) THEN \'1\' WHEN fd."TotalPrice"/ba.price >= 10 THEN ((fd."TotalPrice"/ba.price)::int)::text ELSE round((fd."TotalPrice"/ba.price)::numeric(10,1),1)::text END as "priceRatioText"
FROM basePrice ba, bestIds bi, "durabilityRawData" fd WHERE bi.ops=fd.ops AND bi."DataSize"=fd."DataSize" AND bi."requiredDurability"=fd."requiredDurability" AND bi.bestId=fd.id
AND ba.ops=fd.ops AND ba."DataSize"=fd."DataSize")
SELECT * FROM result;'))
head(durabilityRatioData)

durabilityRatioData$myDura <- ordered(durabilityRatioData$dura, levels=c(1,3,5,11), labels=c("1×9's vs. 3×9's","3×9's Durability", "3×9's vs. 5×9's", "3×9's vs. 11×9's")) # Multi-AZ Deploy. +
plot_dura_ratio <- ggplot(durabilityRatioData, aes(x=factor(DataSize),y=factor(ops),fill=priceRatio)) +
scale_fill_gradientn(colours=c(rgb(48,147,67,maxColorValue=255),rgb(255,221,113,maxColorValue=255),rgb(216,37,38,maxColorValue=255)), values=c(0,0.1,1)) +
    geom_tile(color="black") +
    geom_text(mapping=aes(label=priceRatioText), size=4.5) +
    scale_x_discrete(name = "Dataset Size", label = datasetLabels, expand=c(0,0)) +
    scale_y_discrete(name = "Operations / sec", label = scientific_10, expand=c(0,0)) +
    facet_wrap(vars(myDura)) +
    theme_bw() +
    theme(
        axis.text.x=element_blank(),
        axis.text.y=element_blank(),
        axis.ticks.y=element_blank(),
        axis.ticks.x=element_blank(),
        panel.grid.major = element_blank(),
        panel.grid.minor = element_blank(),
        axis.title=element_blank(),
        strip.text.y = element_blank(),
        strip.text.x = element_text(size = 14.0),
        legend.position = "none"
    )
ggsave(plot = plot_dura_ratio, filename = "durability_part2.pdf", device = cairo_pdf, width = 4.2, height = 4.88) # 4.4 width for one
########################################


########################################
### LATENCIES
########################################
opsVector <- c(10000)
skewVector <- c(0.0)
percentUpdatesVector <- c(0)#,30,100)
datasetSizes <- c(1000)
durabilityLevels <- c(1)#,4,7,11)
latencyLevels <- c(10000,50000,100000,150000,200000,300000,400000,500000)
interAZ <- c(FALSE)
noGroupCommit <- FALSE
latencyDataRaw <- runAllCalcs(opsVector,skewVector, percentUpdatesVector,latencyLevels,datasetSizes, durabilityLevels, interAZ, noGroupCommit)

latencyData <- sqldf(paste0('WITH combinations AS (SELECT "Type",ops,"DataSize","percentUpdates","requiredDurability","requiredLatency" FROM "latencyDataRaw" GROUP BY 1,2,3,4,5,6),
bestIds AS (
  SELECT cd.*,(SELECT fd.id
               FROM "latencyDataRaw" fd
               WHERE fd.ops=cd.ops AND fd."percentUpdates"=cd."percentUpdates" AND fd."DataSize"=cd."DataSize" AND fd."requiredDurability"=cd."requiredDurability" AND fd."requiredLatency"=cd."requiredLatency" AND fd."Type"=cd."Type"
               ORDER BY fd."TotalPrice" ASC, fd."Durability" DESC, fd."OpLatency" ASC, "numSec" ASC, fd."Type" DESC, fd.id ASC LIMIT 1) as bestId FROM combinations cd)
SELECT (CASE WHEN fd."numSec" > 0 THEN (fd."numSec"+1)::text || \'×\' ELSE \'\' END) || replace(fd."Primary",\'-rbpex\',\'\') || \'\n$\' || (case when fd."TotalPrice">=9.9 THEN (fd."TotalPrice"::int)::text when fd."TotalPrice">=0.95 THEN round(fd."TotalPrice"::numeric(10,1),1)::text ELSE round(fd."TotalPrice"::numeric(10,2),2)::text END) as "NamePrice",
    (fd."numSec" + 1) as "numInstances",
    fd."Type",
    fd."requiredDurability" as dura,
    fd."requiredLatency" as lat,
    fd."TotalPrice",
    upper(substr(fd."Type",0,2)) as flett,
    fd."DataSize", fd.ops, fd."percentUpdates"
FROM "latencyDataRaw" fd, bestIds bi
 WHERE bi.ops=fd.ops AND bi."percentUpdates"=fd."percentUpdates" AND bi."DataSize"=fd."DataSize" AND bi."requiredLatency"=fd."requiredLatency" AND bi."requiredDurability"=fd."requiredDurability" AND fd."Type"=bi."Type" AND fd.id=bi.bestId;'))


plot27 <- ggplot(latencyData, aes(x=lat/1000,y=TotalPrice,group=Type,shape=Type,color=Type)) +
    geom_line(linewidth=1.0,alpha=0.7) +
    geom_point(size=2.0) +
    scale_x_reverse(name = "Latency Constraint [µs]", breaks=c(500,400,300,200,100,10),labels=c("unconstrained\n(SSD)", 400,300,200,100,"10\n(RAM)")) +
    scale_y_continuous(name = "$/hour",breaks=c(0,3,6,9,12,15)) +
    theme_bw() +
#    expand_limits(y=0,x=0) +
    scale_color_manual(values = architectureColors, labels=archNames) +
    scale_shape_manual(values = architectureShapes, labels=archNames) +
  theme(
    axis.text.x=element_text(size=12),
    axis.text.y=element_text(size=12),
#    axis.ticks = element_blank(),
    axis.title=element_text(size=12),
#    panel.grid.major = element_blank(),
    panel.grid.minor = element_blank(),
    strip.text.x = element_text(size = 14.0),
    legend.title = element_blank(),
    legend.position = c(0.26,0.68),
    legend.background = element_blank(),
    legend.justification = c(0.6,0.54),
    legend.key.size = unit(5.5, "mm"),
    legend.text = element_text(size=12),
    legend.spacing.y = unit(5, "mm"),
    legend.spacing.x = unit(1, "mm")
  )
ggsave(plot = plot27, filename = "latencies.pdf", device = cairo_pdf, width = 3.8, height = 2.3)


########################################
### SKEW
########################################
########################################
opsVector <- c(1000000)
percentUpdatesVector <- c(0)
zipf <- c(0.0,0.25,0.5,0.7,0.8,1.0,1.2,1.5,2.0)
datasetSizes <- c(1000)
latencyLevels <- c(10000000)#c(1000,10000,100000,1000000)
durabilityLevels <- c(1)
interAZ <- c(FALSE)
noGroupCommit <- FALSE
skewRawData <- runAllCalcs(opsVector,zipf,percentUpdatesVector,latencyLevels,datasetSizes, durabilityLevels, interAZ, noGroupCommit)

skewData <- sqldf(paste0('WITH
combinations AS (SELECT ops,"DataSize","lookupZipf","Type" FROM "skewRawData"
WHERE (ops=1000000) AND "DataSize" = 1024::bigint*1024*1024*1000
GROUP BY ops,"DataSize","lookupZipf","Type"),
bestIds AS (SELECT cd.*,(SELECT fd.id FROM "skewRawData" fd WHERE fd.ops=cd.ops AND fd."lookupZipf"=cd."lookupZipf" AND fd."DataSize"=cd."DataSize" AND fd."Type"=cd."Type" ORDER BY fd."TotalPrice" ASC, fd."Durability" DESC, fd."OpLatency" ASC, "numSec" ASC, fd."Type" ASC, fd.id ASC LIMIT 1) as bestId FROM combinations cd)
SELECT fd."lookupZipf",
      (CASE WHEN fd."numSec" > 0 THEN (fd."numSec"+1)::text || \'×\' ELSE \'\' END) || replace(fd."Primary",\'-rbpex\',\'\') || \'\n$\' || (case when fd."TotalPrice">=9.9 THEN (fd."TotalPrice"::int)::text when fd."TotalPrice">=0.95 THEN round(fd."TotalPrice"::numeric(10,1),1)::text ELSE round(fd."TotalPrice"::numeric(10,2),2)::text END) as "NamePrice",
      upper(substr(fd."Type",0,2)) as flett,
      fd."numSec",
      fd."Type",
      CASE WHEN fd."lookupZipf"=1.0 THEN fd."Type" ELSE \'\' END as label,
      fd."TotalPrice",
      fd."DataSize",
      fd.ops
      FROM "skewRawData" fd, bestIds bi
      WHERE bi.ops=fd.ops AND bi."DataSize"=fd."DataSize" AND fd.id=bi.bestId AND fd."Type"=bi."Type" AND fd."lookupZipf"=bi."lookupZipf"'))


skewData$myZipf <- ordered(skewData$lookupZipf, levels=c(0,0.1,0.5,0.7,0.8,1.0,1.2,1.5,2.0), labels=c("uniform","0.1", "0.5","0.7", "0.8", "1.0","1.2","1.5", "2.0")) # Mul
skewData$myLabel = factor(skewData$label, levels=archLevels, labels=archNames)
plot <- ggplot(skewData, aes(x=lookupZipf,y=TotalPrice,group=Type,shape=Type,color=Type,label=myLabel)) +
    geom_line(linewidth=1.0,alpha=0.7) +
    geom_point(size=2.0) +
    geom_text(size=5.0) +
    scale_shape_manual(values=architectureShapes) +
    scale_x_continuous(name = "Skew (zipf factor)", breaks=c(0,0.5,1.0,1.5,2.0),labels=c("uniform",0.5,1.0,1.5,2.0)) +
    scale_y_continuous(name = "$/hour") +
#    facet_grid(rows=vars(ops), cols=vars(DataSize/1e9), scales = "free") +
    theme_bw() +
    expand_limits(y=0) +
    scale_color_manual(values = architectureColors, labels=archNames) +
#    scale_shape_manual(values = c(0,1,2,3,4,5)) +
#    scale_discrete_manual(aes(shape=Type,color=Type), values = architectureColors, labels=archNames) +
    theme(
    axis.text.x=element_text(size=14),
    axis.text.y=element_text(size=14),
#    axis.ticks = element_blank(),
    axis.title=element_text(size=16),
#    panel.grid.major = element_blank(),
    panel.grid.minor = element_blank(),
    strip.text.x = element_text(size = 14.0),
    legend.title = element_blank(),
    legend.position = "none",
    legend.background = element_blank(),
    legend.justification = c(0.5,0.5),
    legend.key.size = unit(7.5, "mm"),
#    legend.key = element_blank()#element_rect(color="transparent"),
    legend.text = element_text(size=16),
    legend.spacing.y = unit(5, "mm"),
    legend.spacing.x = unit(2, "mm")
    )
ggsave(plot = plot, filename = "skew.pdf", device = cairo_pdf, width = 5, height = 2.6) # 4.4 width for one
########################################


########################################
### UPDATE RATIO
########################################
########################################
opsVector <- c(1000,10000,100000,1000000,10000000,100000000)
skewVector <- c(0.0)
percentUpdatesVector <- c(0,10,30,100)#,30,100)
datasetSizes <- c(10,100,1000,10000,100000)
latencyLevels <- c(10000000)#c(1000,10000,100000,1000000)
durabilityLevels <- c(3)#,4,7,11)
interAZ <- c(FALSE)
noGroupCommit <- FALSE
updateRatioRaw <- runAllCalcs(opsVector,skewVector, percentUpdatesVector,latencyLevels,datasetSizes, durabilityLevels, interAZ, noGroupCommit)

updateRatioData <- sqldf(paste0('WITH
combinations AS (SELECT ops,"DataSize","requiredDurability","percentUpdates" FROM "updateRatioRaw" GROUP BY 1,2,3,4),
bestIds AS (SELECT cd.*,(SELECT fd.id FROM "updateRatioRaw" fd WHERE fd.ops=cd.ops AND fd."DataSize"=cd."DataSize" AND fd."percentUpdates"=cd."percentUpdates" ORDER BY fd."TotalPrice" ASC, fd."Durability" DESC, fd."OpLatency" ASC, "numSec" ASC, fd."Type" ASC, fd.id ASC LIMIT 1) as bestId FROM combinations cd),
basePrice AS (SELECT bi.*,fd."TotalPrice" as price FROM bestIds bi, "updateRatioRaw" fd WHERE bi."percentUpdates"=30 AND bi.ops=fd.ops AND bi."DataSize"=fd."DataSize" AND bi.bestId=fd.id AND bi."percentUpdates"=fd."percentUpdates"),
result AS (SELECT fd."Type",fd."requiredDurability" as dura,fd.ops, fd."DataSize",fd."TotalPrice", fd."percentUpdates",
       CASE WHEN fd."TotalPrice"/ba.price >= 10 THEN ((fd."TotalPrice"/ba.price)::int) ELSE round(fd."TotalPrice"/ba.price::numeric(15,10),1) END as "priceRatio",
       CASE WHEN (fd."TotalPrice"/ba.price <= 1.05 AND fd."TotalPrice"/ba.price >= 0.95) THEN \'1\' WHEN fd."TotalPrice"/ba.price >= 10 THEN ((fd."TotalPrice"/ba.price)::int)::text ELSE round((fd."TotalPrice"/ba.price)::numeric(10,1),1)::text END as "priceRatioText"
FROM basePrice ba, bestIds bi, "updateRatioRaw" fd WHERE bi.ops=fd.ops AND bi."DataSize"=fd."DataSize" AND bi."requiredDurability"=fd."requiredDurability" AND bi.bestId=fd.id
AND ba.ops=fd.ops AND ba."DataSize"=fd."DataSize" AND bi."percentUpdates"=fd."percentUpdates")
SELECT * FROM result;'))
head(updateRatioData)
updateRatioData$myPercent = ordered(updateRatioData$percentUpdates,levels=c('0','10','30','100'),labels=c('100% Lookups','10% Updates','30% Updates','100% Updates'))
plot_update_ratio <- ggplot(updateRatioData, aes(x=factor(DataSize),y=factor(ops),fill=priceRatio)) +
scale_fill_gradientn(colours=c(rgb(48,147,67,maxColorValue=255),rgb(255,221,113,maxColorValue=255),rgb(216,37,38,maxColorValue=255)), values=c(0,0.1,1)) +
    geom_tile(color="black") +
    geom_text(mapping=aes(label=priceRatioText), size=4.5) +
    scale_x_discrete(name = "", label = datasetLabels, expand=c(0,0)) +
    scale_y_discrete(name = "", label = scientific_10, expand=c(0,0)) +
    facet_wrap(nrow=1, vars(myPercent)) +
    theme_bw() +
    theme(
        axis.text.x=element_blank(),
        axis.text.y=element_blank(),
        axis.ticks.y=element_blank(),
        axis.ticks.x=element_blank(),
        panel.grid.major = element_blank(),
        panel.grid.minor = element_blank(),
        axis.title=element_blank(),
        strip.text.x = element_text(size = 15.0),
        legend.position = "none"
    )
ggsave(plot = plot_update_ratio, filename = "update_ratio_price.pdf", device = cairo_pdf, width = 6.3, height = 1.7) # 4.4 width for one

########################################
### AVAILABILITY
########################################
opsVector <- c(1000,10000)
skewVector <- c(0.0)
percentUpdatesVector <- c(30)#,30,100)
datasetSizes <- c(1000)
latencyLevels <- c(10000000)#c(1000,10000,100000,1000000)
durabilityLevels <- c(3)#,4,7,11)
interAZ <- c(FALSE)
noGroupCommit <- FALSE
availNone <- runAllCalcs(opsVector,skewVector, percentUpdatesVector,latencyLevels,datasetSizes, durabilityLevels, c(FALSE), noGroupCommit, 0,0)
availNone$workload = "none"
availSec <- runAllCalcs(opsVector,skewVector, percentUpdatesVector,latencyLevels,datasetSizes, durabilityLevels, c(FALSE), noGroupCommit, 1,1)
availSec$workload = "secondary"
availSecMulti <- runAllCalcs(opsVector,skewVector, percentUpdatesVector,latencyLevels,datasetSizes, durabilityLevels, c(TRUE), noGroupCommit, 1,1)
availSecMulti$workload = "multiaz"
availData <- bind_rows(availNone, availSec, availSecMulti)

costDistData <- sqldf(paste0('WITH combinations AS (SELECT ops,"DataSize","Type","workload" FROM "availData" GROUP BY 1,2,3,4),
vars(x) AS (SELECT * FROM (VALUES (\'prim\'),(\'sec\'),(\'storage\'),(\'page\'),(\'log\'),(\'network\'),(\'ebs\'))),
bestIds AS (SELECT cd.*,(SELECT fd.id FROM "availData" fd WHERE fd.ops=cd.ops AND fd."Type"=cd."Type" AND fd."DataSize"=cd."DataSize" AND fd.workload=cd.workload ORDER BY fd."TotalPrice" ASC, "numSec" ASC, fd."Durability" DESC, fd."Type" DESC, fd.id ASC LIMIT 1) as bestId FROM combinations cd),
result AS (
SELECT
    fd."Type",
    fd."DataSize",
    fd.ops,
    fd.workload,
    fd."TotalPrice",
    v.x as var,
    CASE v.x WHEN \'prim\' THEN replace(fd."Primary",\'-rbpex\',\'\')
             ELSE \'\' END as mylabel,
    CASE v.x WHEN \'prim\' THEN fd."PrimPrice"
             WHEN \'sec\' THEN fd."SecPrice"
             WHEN \'storage\' THEN (CASE WHEN fd."Type"=\'aurora\' THEN fd."PageSvcPrice" ELSE 0.0 END)
             WHEN \'page\' THEN (CASE WHEN fd."Type"=\'socrates\' THEN fd."PageSvcPrice" ELSE 0.0 END)
             WHEN \'ebs\' THEN fd."EBSPrice"
             WHEN \'log\' THEN fd."LogSvcPrice"
             WHEN \'network\' THEN fd."NetworkPrice"
             ELSE 999.9 END as cost
FROM "availData" fd, vars v, bestIds bi WHERE bi.ops=fd.ops AND bi."DataSize"=fd."DataSize" AND fd.id=bi.bestId AND fd."Type"=bi."Type" AND fd.workload=bi.workload)
SELECT * FROM result UNION ALL (SELECT * FROM (VALUES (\'rbd\',0,1000,\'secondary\',0.0,\'\',\'\',0.0),(\'rbd\',0,1000,\'multiaz\',0.0,\'\',\'\',0.0),(\'hadr\',0,1000,\'none\',0.0,\'\',\'\',0.0)));'))
componentColors <- c(  "prim" = reduceSat(rgb(144,168,197, maxColorValue = 255)),
                        "sec" = reduceSat(rgb(148,103,189, maxColorValue = 255)),
                        "ebs" = reduceSat(rgb(31,119,180, maxColorValue = 255)),
                        "page" = reduceSat(rgb(188,189,34, maxColorValue = 255)),
                        "storage" = reduceSat(rgb(68,160,68, maxColorValue = 255)),
                        "log" = reduceSat(rgb(228,229,108, maxColorValue = 255)),
                        "network" = reduceSat(rgb(241,163,64, maxColorValue = 255))
                         )
componentLabels <- c("prim" = "Primary",
                        "sec" = "Secondary",
                        "ebs" = "EBS",
                        "page" = "Page Service",
                        "storage" = "Storage Service",
                        "log" = "Log Service",
                        "network" = "Network"
                        )
archNames2 <- c(
    "classic" = "Classic",
    "inmem" = "In-Memory",
    "rbd" = "RBD",
    "hadr" = "HADR",
    "aurora" = "Aurora-like",
    "socrates" = "Socrates-like"
)
costDistData$newOps <- factor(costDistData$ops, levels=c(1000,10000), labels=c("1k  Ops/sec", "10k  Ops/sec"))
costDistData$newVar <- ordered(costDistData$var, levels=rev(c("prim","ebs","storage","page","log","network","sec")))
costDistData$newType <- ordered(costDistData$Type, levels=c("classic","inmem","rbd","hadr","aurora","socrates"), labels=archNames2)
costDistData$newWorkload <- ordered(costDistData$workload, levels=c("none","secondary","multiaz"), labels=c("none" = "No Secondary","secondary"="Node Failure Safe\n(Hot Standby)","multiaz"="AZ Failure Safe\n(Multi-AZ Deploy.)"))
plot_cost_comp <- ggplot(costDistData, aes(x=newType, y=cost,fill=newVar)) +
    geom_col() +
#    geom_text(aes(label=mylabel,y=-0.1), size=2.7, color="black") +
                                        #    facet_grid(rows=vars(DataSize),cols=vars(ops), scales="free", labeller=labeller(ops=scientific_10, DataSize=datasetLabels)) +
    facet_grid(cols=vars(newWorkload),rows=vars(newOps), scales="free") +
    scale_x_discrete(name = "Architecture") +
    scale_fill_manual(values = componentColors, name = "", labels=componentLabels) +
    scale_y_continuous(name = "$/hour") +
#    coord_cartesian(ylim=c(0,1.0)) +
    theme_bw() +
    theme(
        axis.text.x=element_text(size=12, angle=40, hjust=1),
        axis.text.y=element_text(size=12),
        axis.title.x=element_blank(),
#        axis.ticks.y=element_blank(),
#        panel.grid.major = element_blank(),
        panel.grid.minor = element_blank(),
        axis.title=element_text(size=12),
        strip.text.y = element_text(size = 12.0),
        strip.text.x = element_text(size = 12.0),
        legend.position = "top",
        legend.text = element_text(size=12),
        legend.spacing.y = unit(5, "mm"),
        legend.spacing.x = unit(1, "mm")
    ) #+ guides(fill = guide_legend(nrow = 1))
ggsave(plot = plot_cost_comp, filename = "availability.pdf", device = cairo_pdf, width = 5.4, height = 4.5) # 4.4 width for one
