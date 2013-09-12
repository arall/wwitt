
import re

def detectCMS(entries):

	for web in entries:
		robots_cms_type = "unknown"
		generator_cms_type = "unknown"
		header_cms_type = "unknown"

		# Analyze robots.txt
		####################
		if web['robots']:
			robots = web['robots'].lower()
			# Wordpress contains "wp-admin" "wp-includes"
			if "wp-admin" in robots and "wp-includes" in robots:
				robots_cms_type = "wordpress"

			# Joomla contains a "Joomla" banner
			if (" joomla " in robots) or \
				("/cache/" in robots and "/components/" in robots and "/modules/" in robots and "/installation/" in robots):
				robots_cms_type = "joomla"

			# Xwiki contains paths in the robots
			if "/xwiki/" in robots:
				robots_cms_type = "xwiki"

			# Drupal has many rules to disallow user paths
			if "disallow: /?q=user/password/" in robots:
				robots_cms_type = "drupal"

			# phpBB has modrewrite rules also
			if "disallow: /privmsg" in robots and "disallow: /modcp" in robots and "disallow: /post" in robots:
				robots_cms_type = "phpbb"
			if "disallow: /privmsg.php" in robots and "disallow: /modcp.php" in robots and "disallow: /posting.php" in robots:
				robots_cms_type = "phpbb"

			# Prestashop contains a link to the web and some funny disallows
			if "prestashop.com" in robots or "disallow: /*id_currency=" in robots:
				robots_cms_type = "prestashop"


		# Analyze index generator html field
		####################
		if web['index']:
			index = web['index'].lower()
			generator = re.search('(<meta.*?name="generator".*?>)',index,re.IGNORECASE)
			if generator:
				generator = generator.group(1)
				content = re.search('<meta.*?content="(.*?)".*?/>',generator,re.IGNORECASE)
				if content: content = content.group(1)
				else: content = ""

				for keyword in ["drupal","wordpress","joomla","xwiki","mediawiki"]:
					if keyword in content:
						generator_cms_type = keyword
						break

		# Analyze index header
		####################
		if web['header']:
			header = web['header'].lower()
			if re.search("set-cookie:[^\n]*phpbb",header):
				header_cms_type = 'phpbb'

		


		print ("Web " + web['host'] + " is " + robots_cms_type + " / " + generator_cms_type)


