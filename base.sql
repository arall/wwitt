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
  `ip` int(10) unsigned NOT NULL,
  `reverseIpStatus` int(1) NOT NULL DEFAULT '0',
  `hostname` varchar(50) DEFAULT NULL,
  `os` varchar(50) DEFAULT NULL,
  `dateAdd` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `dateUpdate` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  PRIMARY KEY (`ip`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.


-- Dumping structure for table wwitt.services
DROP TABLE IF EXISTS `services`;
CREATE TABLE IF NOT EXISTS `services` (
  `ip` int(10) unsigned NOT NULL,
  `port` int(10) NOT NULL,
  `filtered` int(1) DEFAULT NULL,
  `head` text,
  `name` varchar(50) DEFAULT NULL,
  `protocol` varchar(50) DEFAULT NULL,
  `product` varchar(50) DEFAULT NULL,
  `version` varchar(50) DEFAULT NULL,
  `info` varchar(50) DEFAULT NULL,
  `dateAdd` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`ip`,`port`),
  UNIQUE KEY `ipId_port` (`ip`,`port`),
  CONSTRAINT `services_ip` FOREIGN KEY (`ip`) REFERENCES `hosts` (`ip`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.


-- Dumping structure for table wwitt.users
DROP TABLE IF EXISTS `users`;
CREATE TABLE IF NOT EXISTS `users` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `username` varchar(16) NOT NULL,
  `password` varchar(512) NOT NULL DEFAULT '0',
  `lastvisitDate` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.


-- Dumping structure for view wwitt.viewHosts
DROP VIEW IF EXISTS `viewHosts`;
-- Creating temporary table to overcome VIEW dependency errors
CREATE TABLE `viewHosts` (
	`ipAdress` VARCHAR(31) NULL COLLATE 'utf8_general_ci',
	`ip` INT(10) UNSIGNED NOT NULL,
	`reverseIpStatus` INT(1) NOT NULL,
	`hostname` VARCHAR(50) NULL COLLATE 'latin1_swedish_ci',
	`os` VARCHAR(50) NULL COLLATE 'latin1_swedish_ci',
	`dateAdd` TIMESTAMP NULL,
	`dateUpdate` TIMESTAMP NOT NULL,
	`totalServices` BIGINT(21) NULL,
	`totalVirtualhosts` BIGINT(21) NULL
) ENGINE=MyISAM;


-- Dumping structure for table wwitt.virtualhosts
DROP TABLE IF EXISTS `virtualhosts`;
CREATE TABLE IF NOT EXISTS `virtualhosts` (
  `ip` int(10) unsigned NOT NULL,
  `host` varchar(50) NOT NULL,
  `url` varchar(512) DEFAULT NULL,
  `head` text,
  `index` mediumtext,
  `robots` mediumtext,
  `dateAdd` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`ip`,`host`),
  UNIQUE KEY `ipId_host` (`ip`,`host`),
  CONSTRAINT `virtualHosts_ip` FOREIGN KEY (`ip`) REFERENCES `hosts` (`ip`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.


-- Dumping structure for table wwitt.vulns
DROP TABLE IF EXISTS `vulns`;
CREATE TABLE IF NOT EXISTS `vulns` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `name` varchar(250) DEFAULT NULL,
  `type` int(11) NOT NULL DEFAULT '0',
  `port` int(11) NOT NULL DEFAULT '0',
  `exploitModule` varchar(250) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.


-- Dumping structure for table wwitt.vulns_hosts
DROP TABLE IF EXISTS `vulns_hosts`;
CREATE TABLE IF NOT EXISTS `vulns_hosts` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `ip` int(10) unsigned NOT NULL DEFAULT '0',
  `vulnId` int(10) NOT NULL DEFAULT '0',
  `port` int(10) NOT NULL DEFAULT '0',
  `virtualhostId` int(10) NOT NULL DEFAULT '0',
  `status` int(1) NOT NULL DEFAULT '0',
  `data` text,
  `dateAdd` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `dateUpdate` timestamp NULL DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `vuln_hosts_ip` (`ip`),
  CONSTRAINT `vuln_hosts_ip` FOREIGN KEY (`ip`) REFERENCES `hosts` (`ip`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.


-- Dumping structure for view wwitt.viewHosts
DROP VIEW IF EXISTS `viewHosts`;
-- Removing temporary table and create final VIEW structure
DROP TABLE IF EXISTS `viewHosts`;
CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `viewHosts` AS select inet_ntoa(`hosts`.`ip`) AS `ipAdress`,`hosts`.`ip` AS `ip`,`hosts`.`reverseIpStatus` AS `reverseIpStatus`,`hosts`.`hostname` AS `hostname`,`hosts`.`os` AS `os`,`hosts`.`dateAdd` AS `dateAdd`,`hosts`.`dateUpdate` AS `dateUpdate`,(select count(`services`.`ip`) from `services` where (`services`.`ip` = `hosts`.`ip`)) AS `totalServices`,(select count(`virtualhosts`.`ip`) from `virtualhosts` where (`virtualhosts`.`ip` = `hosts`.`ip`)) AS `totalVirtualhosts` from `hosts`;
/*!40101 SET SQL_MODE=IFNULL(@OLD_SQL_MODE, '') */;
/*!40014 SET FOREIGN_KEY_CHECKS=IF(@OLD_FOREIGN_KEY_CHECKS IS NULL, 1, @OLD_FOREIGN_KEY_CHECKS) */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
