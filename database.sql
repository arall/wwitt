# --------------------------------------------------------
# Host:                         127.0.0.1
# Server version:               5.5.31-0+wheezy1
# Server OS:                    debian-linux-gnu
# HeidiSQL version:             6.0.0.3603
# Date/time:                    2013-09-01 07:00:50
# --------------------------------------------------------

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET NAMES utf8 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;

# Dumping structure for table wwitt.heads
DROP TABLE IF EXISTS `heads`;
CREATE TABLE IF NOT EXISTS `heads` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `ipId` int(10) DEFAULT NULL,
  `port` int(10) DEFAULT NULL,
  `head` text,
  `dateAdd` datetime DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `ipId_port` (`ipId`,`port`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

# Data exporting was unselected.


# Dumping structure for table wwitt.hosts
DROP TABLE IF EXISTS `hosts`;
CREATE TABLE IF NOT EXISTS `hosts` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `ip` varchar(50) DEFAULT NULL,
  `status` int(11) DEFAULT NULL,
  `dateAdd` datetime DEFAULT NULL,
  `dateUpdate` datetime DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

# Data exporting was unselected.
/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
