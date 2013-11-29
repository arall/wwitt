-- --------------------------------------------------------
-- Host:                         localhost
-- Server version:               5.5.32-0ubuntu0.13.04.1 - (Ubuntu)
-- Server OS:                    debian-linux-gnu
-- HeidiSQL Version:             8.0.0.4396
-- --------------------------------------------------------

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET NAMES utf8 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;

-- Dumping structure for table wwitt.hosts
DROP TABLE IF EXISTS `hosts`;
CREATE TABLE IF NOT EXISTS `hosts` (
  `ip` int(11) NOT NULL,
  `status` int(11) DEFAULT NULL,
  `hostname` varchar(50) DEFAULT NULL,
  `os` varchar(50) DEFAULT NULL,
  `dateAdd` datetime DEFAULT NULL,
  `dateUpdate` datetime DEFAULT NULL,
  PRIMARY KEY (`ip`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.


-- Dumping structure for table wwitt.services
DROP TABLE IF EXISTS `services`;
CREATE TABLE IF NOT EXISTS `services` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `ip` int(10) DEFAULT NULL,
  `port` int(10) DEFAULT NULL,
  `status` int(10) DEFAULT NULL,
  `head` text,
  `name` varchar(50) DEFAULT NULL,
  `protocol` varchar(50) DEFAULT NULL,
  `product` varchar(50) DEFAULT NULL,
  `version` varchar(50) DEFAULT NULL,
  `info` varchar(50) DEFAULT NULL,
  `dateAdd` datetime DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `ipId_port` (`ip`,`port`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.


-- Dumping structure for table wwitt.users
DROP TABLE IF EXISTS `users`;
CREATE TABLE IF NOT EXISTS `users` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `username` varchar(16) NOT NULL,
  `password` varchar(512) NOT NULL DEFAULT '0',
  `lastvisitDate` datetime NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.


-- Dumping structure for view wwitt.viewHosts
DROP VIEW IF EXISTS `viewHosts`;
-- Creating temporary table to overcome VIEW dependency errors
CREATE TABLE `viewHosts` (
	`ipAdress` VARCHAR(31) NULL COLLATE 'utf8_general_ci',
	`ip` INT(11) NOT NULL,
	`status` INT(11) NULL,
	`hostname` VARCHAR(50) NULL COLLATE 'latin1_swedish_ci',
	`os` VARCHAR(50) NULL COLLATE 'latin1_swedish_ci',
	`dateAdd` DATETIME NULL,
	`dateUpdate` DATETIME NULL,
	`totalServices` BIGINT(21) NULL,
	`totalVirtualhosts` BIGINT(21) NULL
) ENGINE=MyISAM;


-- Dumping structure for table wwitt.virtualhosts
DROP TABLE IF EXISTS `virtualhosts`;
CREATE TABLE IF NOT EXISTS `virtualhosts` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `ip` int(10) DEFAULT '0',
  `host` varchar(50) DEFAULT NULL,
  `url` varchar(512) DEFAULT NULL,
  `head` text,
  `index` mediumtext,
  `robots` mediumtext,
  `dateAdd` datetime DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `ipId_host` (`ip`,`host`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.


-- Dumping structure for table wwitt.vulns
DROP TABLE IF EXISTS `vulns`;
CREATE TABLE IF NOT EXISTS `vulns` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `name` varchar(250) DEFAULT NULL,
  `type` int(11) NOT NULL DEFAULT '0',
  `exploitModule` varchar(250) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.


-- Dumping structure for table wwitt.vulns_hosts
DROP TABLE IF EXISTS `vulns_hosts`;
CREATE TABLE IF NOT EXISTS `vulns_hosts` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `ip` int(10) NOT NULL DEFAULT '0',
  `vulnId` int(10) NOT NULL DEFAULT '0',
  `port` int(10) NOT NULL DEFAULT '0',
  `virtualhostId` int(10) NOT NULL DEFAULT '0',
  `status` int(10) NOT NULL DEFAULT '0',
  `data` text,
  `dateAdd` datetime DEFAULT NULL,
  `dateUpdate` datetime DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.

INSERT INTO `users` (`username`, `password`) VALUES ('admin', '0c7540eb7e65b553ec1ba6b20de79608');