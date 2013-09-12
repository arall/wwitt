
import re

def detectCMS(entries):

	for web in entries:
		robots_cms_type = "unknown"
		generator_cms_type = "unknown"

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

		# Analyze index generator field
		####################
		index = web['index']
		if index:
			generator = re.match('<meta.*?name="generator".*?/>',index,re.IGNORECASE)
			if generator:
				generator = generator.group()
				content = re.match('<meta.*?content="(.*)".*?/>',generator,re.IGNORECASE)

				for keywork in ["drupal","wordpress","joomla","xwiki","mediawiki"]:
					if keyword in content:
						generator_cms_type = keyword
						break

		print ("Web " + web['host'] + " is " + robots_cms_type + " / " + generator_cms_type)


