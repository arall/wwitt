-- --------------------------------------------------------
-- Host:                         127.0.0.1
-- Server version:               5.5.35-0ubuntu0.13.10.2 - (Ubuntu)
-- Server OS:                    debian-linux-gnu
-- HeidiSQL Version:             8.3.0.4694
-- --------------------------------------------------------

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET NAMES utf8 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;

-- Dumping structure for table wwitt.hosts
DROP TABLE IF EXISTS `hosts`;
CREATE TABLE IF NOT EXISTS `hosts` (
  `ip` int(10) unsigned NOT NULL,
  `status` tinyint(255) unsigned NOT NULL DEFAULT '0',
  `dateInsert` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `dateUpdate` timestamp NULL DEFAULT NULL,
  PRIMARY KEY (`ip`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Dumping data for table wwitt.hosts: ~0 rows (approximately)
DELETE FROM `hosts`;
/*!40000 ALTER TABLE `hosts` DISABLE KEYS */;
/*!40000 ALTER TABLE `hosts` ENABLE KEYS */;


-- Dumping structure for table wwitt.services
DROP TABLE IF EXISTS `services`;
CREATE TABLE IF NOT EXISTS `services` (
  `ip` int(10) unsigned NOT NULL,
  `port` smallint(5) unsigned NOT NULL,
  `filtered` int(1) DEFAULT NULL,
  `head` text,
  `name` varchar(50) DEFAULT NULL,
  `protocol` varchar(50) DEFAULT NULL,
  `version` varchar(50) DEFAULT NULL,
  `dateInsert` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`ip`,`port`),
  UNIQUE KEY `ipId_port` (`ip`,`port`),
  CONSTRAINT `services_ip` FOREIGN KEY (`ip`) REFERENCES `hosts` (`ip`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Dumping data for table wwitt.services: ~0 rows (approximately)
DELETE FROM `services`;
/*!40000 ALTER TABLE `services` DISABLE KEYS */;
/*!40000 ALTER TABLE `services` ENABLE KEYS */;


-- Dumping structure for table wwitt.users
DROP TABLE IF EXISTS `users`;
CREATE TABLE IF NOT EXISTS `users` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `username` varchar(16) NOT NULL,
  `password` varchar(512) NOT NULL DEFAULT '0',
  `lastvisitDate` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `dateInsert` datetime DEFAULT NULL,
  `dateUpdate` datetime DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=latin1;

-- Dumping data for table wwitt.users: ~1 rows (approximately)
DELETE FROM `users`;
/*!40000 ALTER TABLE `users` DISABLE KEYS */;
INSERT INTO `users` (`id`, `username`, `password`, `lastvisitDate`, `dateInsert`, `dateUpdate`) VALUES
	(1, 'admin', '0c7540eb7e65b553ec1ba6b20de79608', '2014-03-10 22:15:27', '2014-03-10 22:13:46', '2014-03-10 22:15:27');
/*!40000 ALTER TABLE `users` ENABLE KEYS */;


-- Dumping structure for table wwitt.virtualhosts
DROP TABLE IF EXISTS `virtualhosts`;
CREATE TABLE IF NOT EXISTS `virtualhosts` (
  `host` varchar(50) NOT NULL,
  `status` tinyint(255) unsigned NOT NULL DEFAULT '0',
  `url` varchar(512) DEFAULT NULL,
  `head` text,
  `index` mediumtext,
  `robots` mediumtext,
  `dateInsert` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`host`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Dumping data for table wwitt.virtualhosts: ~0 rows (approximately)
DELETE FROM `virtualhosts`;
/*!40000 ALTER TABLE `virtualhosts` DISABLE KEYS */;
/*!40000 ALTER TABLE `virtualhosts` ENABLE KEYS */;


-- Dumping structure for table wwitt.vulns
DROP TABLE IF EXISTS `vulns`;
CREATE TABLE IF NOT EXISTS `vulns` (
  `module` varchar(50) NOT NULL,
  `name` varchar(50) DEFAULT NULL,
  `type` int(11) NOT NULL DEFAULT '0',
  `protocol` varchar(50) NOT NULL DEFAULT '0',
  `webappName` varchar(50) DEFAULT NULL,
  `minVersion` varchar(50) DEFAULT '0',
  `maxVersion` varchar(50) DEFAULT '0',
  `dateInsert` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`module`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Dumping data for table wwitt.vulns: ~3 rows (approximately)
DELETE FROM `vulns`;
/*!40000 ALTER TABLE `vulns` DISABLE KEYS */;
INSERT INTO `vulns` (`module`, `name`, `type`, `protocol`, `webappName`, `minVersion`, `maxVersion`, `dateInsert`) VALUES
	('httpWeakLogin', 'HTTP Weak Login', 1, 'http', NULL, '0', '0', NULL),
	('sshWeakLogin', 'SSH Weak Login', 1, 'ssh', NULL, '0', '0', NULL),
	('telnetWeakLogin', 'Telnet Weak Login', 1, 'telnet', NULL, '0', '0', NULL);
/*!40000 ALTER TABLE `vulns` ENABLE KEYS */;


-- Dumping structure for table wwitt.vulns_hosts
DROP TABLE IF EXISTS `vulns_hosts`;
CREATE TABLE IF NOT EXISTS `vulns_hosts` (
  `ip` int(10) unsigned NOT NULL DEFAULT '0',
  `vuln` varchar(50) NOT NULL DEFAULT '0',
  `status` tinyint(255) NOT NULL DEFAULT '0',
  `webappId` int(10) NOT NULL DEFAULT '0',
  `data` text,
  `dateInsert` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `dateUpdate` timestamp NULL DEFAULT NULL,
  PRIMARY KEY (`ip`,`vuln`),
  UNIQUE KEY `ip_vuln` (`ip`,`vuln`),
  CONSTRAINT `vuln_hosts_ip` FOREIGN KEY (`ip`) REFERENCES `hosts` (`ip`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Dumping data for table wwitt.vulns_hosts: ~0 rows (approximately)
DELETE FROM `vulns_hosts`;
/*!40000 ALTER TABLE `vulns_hosts` DISABLE KEYS */;
/*!40000 ALTER TABLE `vulns_hosts` ENABLE KEYS */;


-- Dumping structure for table wwitt.webapps
DROP TABLE IF EXISTS `webapps`;
CREATE TABLE IF NOT EXISTS `webapps` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `host` varchar(50) NOT NULL,
  `url` varchar(512) NOT NULL,
  `name` varchar(128) NOT NULL,
  `version` varchar(50) DEFAULT NULL,
  `dateInsert` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `webapps_host` (`host`),
  CONSTRAINT `webapps_host` FOREIGN KEY (`host`) REFERENCES `virtualhosts` (`host`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Dumping data for table wwitt.webapps: ~0 rows (approximately)
DELETE FROM `webapps`;
/*!40000 ALTER TABLE `webapps` DISABLE KEYS */;
/*!40000 ALTER TABLE `webapps` ENABLE KEYS */;
/*!40101 SET SQL_MODE=IFNULL(@OLD_SQL_MODE, '') */;
/*!40014 SET FOREIGN_KEY_CHECKS=IF(@OLD_FOREIGN_KEY_CHECKS IS NULL, 1, @OLD_FOREIGN_KEY_CHECKS) */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
